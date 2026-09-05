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

#include "highscoremanager.h"
#include "shaderstuff.h"
#include "frozenbubble.h"
#include "ttftext.h"
#include "platform.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <string>
#include <utility>

struct HighscoreData {
    int level;
    float time;
    std::string name;
    int picId;
    TTFText layoutText;
    bool newHighscore = false;

    std::string formatTime(){
        int min = time / 60;
        int sec = time - min*60;
        char time[8];
        snprintf(time, sizeof(time), "%d'%02d\"", min, sec);
        return std::string(time);
    }

    void RefreshTextStatus(SDL_Renderer *rend, TTF_Font * /*fnt*/){
        layoutText.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 16);
        layoutText.UpdateColor({255, 255, 255, 255},  {0, 0, 0, 255});
        layoutText.UpdateAlignment(TTF_HORIZONTAL_ALIGN_CENTER);
        if (newHighscore) layoutText.UpdateStyle(TTF_STYLE_BOLD);
        std::string data = (name.size() > 12 ? name.substr(0, 9) + "..." : name) + "\n" + (level > 100 ? "won!" : "level " + std::to_string(level)) + "\n" + formatTime();
        layoutText.UpdateText(rend, data.c_str(), 0);
    }
};

// Two independent top-10 tables, indexed by HighscoreManager::InputMethod:
// [0] keyboard/gamepad, [1] mouse/touch. Kept as two vectors rather than one
// vector with a method field on each entry so every existing index-based
// access (CreateLevelImages' layout math, the fixed on-screen slot grid in
// RenderScoreScreen) stays exactly the same per table -- only which vector
// they iterate changes.
std::vector<HighscoreData> levelsetScores[2];

// The score screen's two tab boxes, KEYBOARD/GAMEPAD and MOUSE/TOUCH -- one
// fixed layout shared by RenderScoreScreen (drawing) and HandleInput
// (hit-testing a click/tap), so the two can never drift out of sync with
// each other. Logical (640x480) canvas coordinates.
static SDL_Rect ScoreTrackTabRect(int track) {
    constexpr int w = 160, h = 26, gap = 10;
    constexpr int totalW = w * 2 + gap;
    constexpr int x0 = 640 / 2 - totalW / 2;
    constexpr int y = 8;
    return { x0 + track * (w + gap), y, w, h };
}

HighscoreManager *HighscoreManager::ptrInstance = NULL;

HighscoreManager *HighscoreManager::Instance(SDL_Renderer *rend)
{
    if(ptrInstance == NULL)
        ptrInstance = new HighscoreManager(rend);
    return ptrInstance;
}

void HighscoreManager::LoadLevelsetHighscores(const char *path, int track) {
    std::ifstream scoreSet(path);
    std::string curLine;

    if(scoreSet.is_open())
    {
        std::string curChar;
        while(std::getline(scoreSet, curLine))
        {

            int task = 0;
            if (!curLine.empty())
            {
                std::stringstream ss(curLine);
                HighscoreData hs;
                // stoi/stof throw on a non-numeric or out-of-range field. This
                // runs during construction, so an uncaught throw took the whole
                // client down before the window appeared -- over a corrupt file
                // in the user's own preferences directory. Drop the bad line and
                // keep the rest of the table instead. The entry is only appended
                // once every field has parsed, so a partial row is not stored.
                try
                {
                    while(std::getline(ss, curChar, ','))
                    {
                        if (task == 0) hs.level = stoi(curChar);
                        else if (task == 1) hs.name = curChar;
                        else if (task == 2) hs.time = stof(curChar);
                        else if (task == 3) {
                            hs.picId = stoi(curChar);
                            levelsetScores[track].push_back(std::move(hs));
                        }
                        task++;
                    }
                }
                catch (const std::logic_error &)
                {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                "Skipping malformed highscore entry in %s: '%s'", path, curLine.c_str());
                }
                task = 0;
            }
        }
    }
    else if (track == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not load highscore levels (%s).", path);
    } else {
        // Not logged as an error: the mouse/touch table (highscores_mouse)
        // is a new file that will not exist yet on any device upgrading from
        // before this feature, and that is the expected, ordinary case for
        // it, not a fault.
        SDL_Log("No highscore levelset file at %s yet.", path);
    }
}

void HighscoreManager::LoadHighscoreLevels(const char *path) {
    std::ifstream lvlSet(path);
    std::string curLine;

    highscoreLevels.clear();
    if(lvlSet.is_open())
    {
        int idx = 0;
        std::string curChar;
        std::array<std::vector<int>, 10> level;
        std::vector<int> line;
        while(std::getline(lvlSet, curLine))
        {
            if (curLine.empty())
            {
                if (idx > 0) {
                    highscoreLevels[(int)highscoreLevels.size()] = level;
                    idx = 0;
                    level = {};
                }
            }
            else {
                std::stringstream ss(curLine);
                // As above: a non-numeric cell must not abort construction.
                try
                {
                    while(std::getline(ss, curChar, ' '))
                    {
                        if(curChar.empty()) continue;
                        else if(curChar == "-") line.push_back(-1);
                        else {
                            line.push_back(stoi(curChar));
                        }
                    }
                }
                catch (const std::logic_error &)
                {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                "Skipping malformed level row in %s: '%s'", path, curLine.c_str());
                    line.clear();
                }

                if (idx < 10) {
                    level[idx] = line;
                }
                line.clear();
                idx++;
            }
        }
        // Flush last level if file doesn't end with a blank line
        if (idx > 0) {
            highscoreLevels[(int)highscoreLevels.size()] = level;
        }
    }
    else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not load highscore levels (%s).", path);
    }
}

void HighscoreManager::AppendToLevels(std::array<std::vector<int>, 10> lvl, int id){
    highscoreLevels[id] = lvl;
    CreateLevelImages();
    SaveNewHighscores();
}

bool HighscoreManager::CheckAndAddScore(int level, float time, InputMethod method) {
    const int track = (int)method;
    std::vector<HighscoreData>& scores = levelsetScores[track];

    // Determine if this score qualifies for top 10 (higher level = better; same level, lower time = better)
    bool qualifies = ((int)scores.size() < 10);
    for (const auto& s : scores) {
        if (level > s.level || (level == s.level && time < s.time)) {
            qualifies = true;
            break;
        }
    }
    if (!qualifies) return false;

    HighscoreData newEntry;
    newEntry.level = level;
    newEntry.time = time;
    newEntry.picId = rand() % 5 + 1;
    newEntry.newHighscore = true;
    newEntry.RefreshTextStatus(rend, highscoreFont);
    scores.push_back(std::move(newEntry));

    // Sort: higher level first, then faster time
    std::sort(scores.begin(), scores.end(), [](const HighscoreData& a, const HighscoreData& b) {
        if (a.level != b.level) return a.level > b.level;
        return a.time < b.time;
    });
    if (scores.size() > 10) scores.resize(10);

    pendingHighscoreTrack = track;
    viewTrack = track;  // so the score screen opens showing the table that was just earned

    SaveNewHighscores();

    return true;
}

HighscoreManager::HighscoreManager(SDL_Renderer *renderer)
{
    rend = renderer;
    gameSettings = GameSettings::Instance();

    backgroundSfc = IMG_Load(ASSET("/gfx/back_one_player.png").c_str());

    for (int i = 1; i <= 8; i++)
    {
        if(gameSettings->colorBlind()) {
            char rel[64];
            snprintf(rel, sizeof(rel), "/gfx/balls/bubble-colourblind-%d.gif", i);
            useBubbles[i - 1] = IMG_Load(ASSET(rel).c_str());
        }
        else {
            char rel[64];
            snprintf(rel, sizeof(rel), "/gfx/balls/bubble-%d.gif", i);
            useBubbles[i - 1] = IMG_Load(ASSET(rel).c_str());
        }
    }

    highscoresBG = IMG_LoadTexture(rend, ASSET("/gfx/back_hiscores.png").c_str());
    highscoreFrame = IMG_LoadTexture(rend, ASSET("/gfx/hiscore_frame.png").c_str());
    headerLevelset = IMG_LoadTexture(rend, ASSET("/gfx/hiscore-levelset.png").c_str());
    headerMptrain = IMG_LoadTexture(rend, ASSET("/gfx/hiscore-mptraining.png").c_str());

    highscoreFont = TTF_OpenFont(ASSET("/gfx/DroidSans.ttf").c_str(), 18);

    voidPanelBG = IMG_LoadTexture(rend, ASSET("/gfx/menu/void_panel.png").c_str());

    panelText.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 15);
    nameInput.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 15);
    trackLabelText.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 15);
    panelText.UpdateAlignment(TTF_HORIZONTAL_ALIGN_CENTER);
    nameInput.UpdateAlignment(TTF_HORIZONTAL_ALIGN_CENTER);
    trackLabelText.UpdateAlignment(TTF_HORIZONTAL_ALIGN_CENTER);
    panelText.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});
    nameInput.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});

    panelText.UpdateText(rend, "Congratulations!\n\nYou got a high score!\n\nEnter name:            \n", 0);
    panelText.UpdatePosition({(640/2) - (panelText.Coords()->w / 2), (480/2) - 120});
    nameInput.UpdatePosition({(640/2) - 45 - (panelText.Coords()->w / 2), (480/2) - 25});

    std::string historypath = gameSettings->prefPath + std::string("highlevelshistory");
    std::string levelsetpath = gameSettings->prefPath + std::string("highscores");
    std::string levelsetMousePath = gameSettings->prefPath + std::string("highscores_mouse");
    LoadHighscoreLevels(historypath.c_str());
    LoadLevelsetHighscores(levelsetpath.c_str(), (int)InputMethod::Keyboard);
    LoadLevelsetHighscores(levelsetMousePath.c_str(), (int)InputMethod::Mouse);

    CreateLevelImages();
}

HighscoreManager::~HighscoreManager(){
    levelsetScores[0].clear();
    levelsetScores[1].clear();
    TTF_CloseFont(highscoreFont);

    // Everything the constructor loaded, plus the per-level thumbnails built by
    // CreateLevelImages. Without this the surfaces and textures outlived the
    // manager -- roughly a megabyte, which shutdown then simply abandoned.
    for (SDL_Texture *&texture : smallBG) {
        if (texture) { SDL_DestroyTexture(texture); texture = nullptr; }
    }
    for (SDL_Texture *texture : {highscoresBG, highscoreFrame, headerLevelset,
                                 headerMptrain, voidPanelBG}) {
        if (texture) SDL_DestroyTexture(texture);
    }
    highscoresBG = highscoreFrame = headerLevelset = nullptr;
    headerMptrain = voidPanelBG = nullptr;

    if (backgroundSfc) { SDL_DestroySurface(backgroundSfc); backgroundSfc = nullptr; }
    for (SDL_Surface *&surface : useBubbles) {
        if (surface) { SDL_DestroySurface(surface); surface = nullptr; }
    }
}

void HighscoreManager::Dispose(){
    SaveNewHighscores();
    // Calling the destructor by hand ran the cleanup but never released the
    // object itself, so the allocation leaked along with everything it still
    // owned. delete does both, exactly once.
    ptrInstance = nullptr;
    delete this;
}

std::string levelToData(std::array<std::vector<int>, 10> lvl) {
    std::string current;
    for (int i = 0; i < 10; i++) {
        if (lvl[i].size() != 8) current += "  ";
        for (size_t j = 0; j < lvl[i].size(); j++) {
            if (lvl[i][j] != -1) current += std::to_string(lvl[i][j]);
            else current += "-";
            if (j < lvl[i].size() - 1) current += "   ";
            else current += "\n";
        }
    }
    current += "\n";
    return current;
}

// Writes contents to path without ever truncating the real file: the whole
// thing is staged beside it and swapped in, so a crash mid-write leaves the
// previous table intact rather than a half-written one.
static bool writeFileAtomically(const std::string &path, const std::string &contents) {
    const std::string temp = path + ".tmp";

    std::ofstream out(temp);
    out << contents;
    out.close();
    if (!out.good()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not stage %s", temp.c_str());
        std::error_code ignored;
        std::filesystem::remove(temp, ignored);
        return false;
    }

    return ReplaceFileAtomically(temp, path);
}

void HighscoreManager::SaveNewHighscores() {
    const std::string historypath = gameSettings->prefPath + std::string("highlevelshistory");
    // Track 0 (keyboard/gamepad) keeps the original filename, so an existing
    // install's table survives this change untouched; track 1 (mouse/touch)
    // is a new file that starts empty until the first mouse/touch highscore.
    const std::string levelsetpath[2] = {
        gameSettings->prefPath + std::string("highscores"),
        gameSettings->prefPath + std::string("highscores_mouse"),
    };

    // Iterate the map rather than indexing it: highscoreLevels is keyed by level
    // id, so operator[](i) over 0..size-1 default-inserted an empty grid for
    // every id that was not present -- and each insertion grew size(), extending
    // the loop that was feeding it. One completed level 17 turned a 1-entry map
    // into 19 entries and wrote 18 empty grids into the history file. Harmless
    // enough when this ran once at shutdown; it now runs on every save.
    std::string historyContents;
    for (const auto& [id, lvl] : highscoreLevels)
    {
        (void)id;
        historyContents += levelToData(lvl);
    }

    std::string levelsetContents[2];
    for (int track = 0; track < 2; track++) {
        std::ostringstream levelsetStream;
        for (const HighscoreData& a : levelsetScores[track]) {
            levelsetStream << a.level << "," << a.name << "," << a.time << "," << a.picId << "\n";
        }
        levelsetContents[track] = levelsetStream.str();
    }

    // Saving eagerly means this runs on every mutation, but a given mutation
    // only ever touches one table -- finishing a level appends to the
    // history, beating a score appends to exactly one of the two score
    // tables. Comparing against what was last written keeps each save to the
    // file that actually changed instead of rewriting all three, and makes
    // the save at shutdown a no-op when nothing happened since. Each
    // mutation still persists on its own, so no caller has to save on
    // another's behalf.
    bool wrote = false;
    if (historyContents != lastSavedHistory &&
        writeFileAtomically(historypath, historyContents)) {
        lastSavedHistory = historyContents;
        wrote = true;
    }
    for (int track = 0; track < 2; track++) {
        if (levelsetContents[track] != lastSavedLevelset[track] &&
            writeFileAtomically(levelsetpath[track], levelsetContents[track])) {
            lastSavedLevelset[track] = levelsetContents[track];
            wrote = true;
        }
    }

    if (wrote) RequestPersistentStorageFlush();
}

void HighscoreManager::CreateLevelImages() {
    SDL_Log("CreateLevelImages: start, highscoreLevels.size=%zu, levelsetScores[0].size=%zu levelsetScores[1].size=%zu",
            highscoreLevels.size(), levelsetScores[0].size(), levelsetScores[1].size());
    SDL_Rect highRect = {(640/2)-128, 51, ((640/2)+128)-((640/2)-128), 340};

    int slot = 0;
    for (auto& [key, lvl] : highscoreLevels) {
        SDL_Log("CreateLevelImages: slot=%d key=%d", slot, key);
        if (slot >= 10) break;
        if (smallBG[slot] != nullptr) { SDL_DestroyTexture(smallBG[slot]); smallBG[slot] = nullptr; }
        SDL_Surface *bigOne = SDL_CreateSurface(640, 480, SDL_PIXELFORMAT_ARGB8888);
        if (!bigOne) { SDL_Log("CreateLevelImages: bigOne null!"); slot++; continue; }
        SDL_Surface *sfc = SDL_CreateSurface(highRect.w/4, highRect.h/4, SDL_PIXELFORMAT_ARGB8888);
        if (!sfc) { SDL_Log("CreateLevelImages: sfc null!"); SDL_DestroySurface(bigOne); slot++; continue; }
        SDL_Log("CreateLevelImages: blitting background (backgroundSfc=%p)", (void*)backgroundSfc);
        if (backgroundSfc) SDL_BlitSurface(backgroundSfc, nullptr, bigOne, nullptr);
        SDL_Log("CreateLevelImages: blitting bubbles");
        for (int j = 0; j < 10; j++){
            int smallerSep = lvl[j].size() % 2 == 0 ? 0 : 32 / 2;
            for (size_t k = 0; k < lvl[j].size(); k++) {
                int bid = lvl[j][k];
                if (bid < 0 || bid > 7 || !useBubbles[bid]) continue;
                SDL_Rect dest = {(smallerSep + 32 * ((int)k)) + 190, (32 * j) + 51, 64, 64};
                SDL_BlitSurface(useBubbles[bid], nullptr, bigOne, &dest);
            }
        }
        SDL_Log("CreateLevelImages: shrinking");
        shrink_(sfc, bigOne, 0, 0, &highRect, 4);
        SDL_Log("CreateLevelImages: creating texture");
        smallBG[slot] = SDL_CreateTextureFromSurface(rend, sfc);
        SDL_DestroySurface(sfc);
        SDL_DestroySurface(bigOne);
        SDL_Log("CreateLevelImages: slot %d done", slot);
        slot++;
    }

    for (int track = 0; track < 2; track++) {
        std::vector<HighscoreData>& scores = levelsetScores[track];
        SDL_Log("CreateLevelImages: refreshing %zu score texts for track %d", scores.size(), track);
        for (size_t i = 0; i < scores.size(); i++) {
            scores[i].RefreshTextStatus(rend, highscoreFont);
            SDL_Rect *c = scores[i].layoutText.Coords();
            // Same 5-columns-per-row, 2-row split as RenderScoreScreen's bgPos
            // (col = i%5, row = i/5) -- this formula used to split rows at
            // "(i+1) % 6 == 0" instead of the correct row boundary of 5, and
            // never wrapped the column back to 0 for row 2 either. That put
            // index 5's text at column 6 (x ~= 618, clipped against the
            // 640-wide canvas) and indices 6-9 further out still (x = 756..
            // 1080), entirely off-canvas -- only the first 5 score entries'
            // text was ever visible, even though CreateLevelImages' own
            // thumbnail loop above already renders all 10 mini-screenshots.
            if (c) {
                int col = (int)i % 5, row = (int)i / 5;
                scores[i].layoutText.UpdatePosition({108 * (col + 1) - c->w/2, 185 * (row + 1)});
            }
        }
    }
    SDL_Log("CreateLevelImages: done");
}

void HighscoreManager::ShowScoreScreen(int ls) {
    lastState = ls;
    FrozenBubble::Instance()->currentState = Highscores;
}

void HighscoreManager::RenderScoreScreen() {
    SDL_RenderTexture(rend, highscoresBG, nullptr, nullptr);

    if (curMode == 0) { // 0 = Levelset
        std::vector<HighscoreData>& scores = levelsetScores[viewTrack];
        for (size_t i = 0; i < scores.size(); i++) {
            int sx = 64, sy = 85;
            if (smallBG[i]) { float fw, fh; SDL_GetTextureSize(smallBG[i], &fw, &fh); sx = (int)fw; sy = (int)fh; }
            // 5 columns per row, 2 rows of 5 (this table holds at most 10
            // entries -- see CheckAndAddScore()'s resize(10)). Both rows use
            // the same per-column x so the grid lines up cleanly; an earlier
            // version of this fix carried over a "-20 * row" left-shift for
            // row 2 from the previous (buggy) formula, which not only left
            // row 2 looking crooked against row 1 but also drifted away from
            // CreateLevelImages' row-independent score-text positions (up to
            // ~23px by the rightmost column, since that formula was never
            // shifted to match).
            int col = (int)i % 5, row = (int)i / 5;
            SDL_Rect bgPos = {105 * col + 85, 80 + 160 * row, sx, sy};
            SDL_Rect framePos = {bgPos.x - 7, bgPos.y - 7, 81, 100};
            { SDL_FRect fr = ToFRect(framePos); SDL_RenderTexture(rend, highscoreFrame, nullptr, &fr); }
            if (smallBG[i]) { SDL_FRect fr = ToFRect(bgPos); SDL_RenderTexture(rend, smallBG[i], nullptr, &fr); }
            { SDL_FRect fr = ToFRect(*scores[i].layoutText.Coords()); SDL_RenderTexture(rend, scores[i].layoutText.Texture(), nullptr, &fr); }
        }

        // Two tab boxes -- click/tap either one to switch tables (see
        // HandleInput's MOUSE_BUTTON_DOWN/FINGER_DOWN cases), or LEFT/RIGHT
        // from a keyboard/gamepad. Own TTFText, not panelText: panelText's
        // style/color is shared mutable state that ShowNewScorePanel()/
        // RenderPanel() need left alone.
        for (int track = 0; track < 2; track++) {
            SDL_Rect box = ScoreTrackTabRect(track);
            bool active = (track == viewTrack);

            SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
            if (active) SDL_SetRenderDrawColor(rend, 255, 196, 64, 90);
            else        SDL_SetRenderDrawColor(rend, 20, 12, 32, 150);
            { SDL_FRect fr = ToFRect(box); SDL_RenderFillRect(rend, &fr); }

            if (active) SDL_SetRenderDrawColor(rend, 255, 218, 92, 240);
            else        SDL_SetRenderDrawColor(rend, 174, 211, 202, 140);
            { SDL_FRect fr = ToFRect(box); SDL_RenderRect(rend, &fr); }

            trackLabelText.UpdateStyle(13, active ? TTF_STYLE_BOLD : TTF_STYLE_NORMAL);
            trackLabelText.UpdateColor(active ? SDL_Color{255, 218, 92, 255} : SDL_Color{174, 211, 202, 255},
                                        {20, 12, 32, 255});
            trackLabelText.UpdateText(rend, track == (int)InputMethod::Mouse ? "MOUSE/TOUCH" : "KEYBOARD", 0);
            trackLabelText.UpdatePosition({box.x + box.w/2 - trackLabelText.Coords()->w/2,
                                           box.y + box.h/2 - trackLabelText.Coords()->h/2});
            { SDL_FRect fr = ToFRect(*trackLabelText.Coords()); SDL_RenderTexture(rend, trackLabelText.Texture(), nullptr, &fr); }
        }
    }

    // Show name entry panel on top when awaiting input
    if (awaitKeyType) {
        RenderPanel();
    }
}

void HighscoreManager::ShowNewScorePanel(int mode) {
    curMode = mode;
    awaitKeyType = true;
    newName.clear();
    panelText.UpdateText(rend, "Congratulations!\n\nYou got a high score!\n\nEnter name:            \n", 0);
    panelText.UpdatePosition({(640/2) - (panelText.Coords()->w / 2), (480/2) - 120});
    SDL_StartTextInput(SDL_GetRenderWindow(rend));
}

void HighscoreManager::RenderPanel() {
    { SDL_FRect fr = ToFRect(voidPanelRct); SDL_RenderTexture(rend, voidPanelBG, nullptr, &fr); }
    { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(rend, panelText.Texture(), nullptr, &fr); }

    if (textTickWait <= 0) {
        if (awaitKeyType) {
            if (showTick) {
                nameInput.UpdateText(rend, newName.c_str(), 0);
                showTick = false;
            }
            else {
                std::string nam = newName + "|";
                nameInput.UpdateText(rend, nam.c_str(), 0);
                showTick = true;
            }
        }
    }
    else textTickWait--;

    { SDL_FRect fr = ToFRect(*nameInput.Coords()); SDL_RenderTexture(rend, nameInput.Texture(), nullptr, &fr); }
}

void HighscoreManager::HandleInput(SDL_Event *e){
    switch(e->type) {
        case SDL_EVENT_KEY_DOWN:
            if(e->key.repeat) break;
            switch(e->key.key) {
                case SDLK_ESCAPE:
                    if (awaitKeyType) {
                        // Cancel name entry - keep any previously entered name
                        awaitKeyType = false;
                        newName.clear();
                        SDL_StopTextInput(SDL_GetRenderWindow(rend));
                        SaveNewHighscores();
                    }
                    FrozenBubble::Instance()->currentState = TitleScreen;
                    break;
                case SDLK_RETURN:
                    if (awaitKeyType) {
                        // Save entered name to most recent new high score entry
                        // -- in whichever table CheckAndAddScore() actually
                        // added it to, not necessarily whichever is on screen
                        // if the player switched tables while this was up.
                        std::vector<HighscoreData>& scores = levelsetScores[pendingHighscoreTrack];
                        for (int i = (int)scores.size() - 1; i >= 0; i--) {
                            if (scores[i].newHighscore) {
                                if (!newName.empty()) {
                                    scores[i].name = newName;
                                    scores[i].RefreshTextStatus(rend, highscoreFont);
                                }
                                scores[i].newHighscore = false;
                                break;
                            }
                        }
                        newName.clear();
                        awaitKeyType = false;
                        SDL_StopTextInput(SDL_GetRenderWindow(rend));
                        SaveNewHighscores();
                        break;
                    }
                    FrozenBubble::Instance()->currentState = TitleScreen;
                    break;
                case SDLK_BACKSPACE:
                    if (awaitKeyType) {
                        if(newName.size() == 0) AudioMixer::Instance()->PlaySFX("stick");
                        else {
                            newName.pop_back();
                            AudioMixer::Instance()->PlaySFX("typewriter");
                        }
                    }
                    break;
                case SDLK_LEFT:
                case SDLK_RIGHT:
                    // Switch between the keyboard/gamepad and mouse/touch
                    // tables. Only while browsing (not while naming a new
                    // entry -- awaitKeyType's own panel has no use for L/R,
                    // and pendingHighscoreTrack already picks the right one).
                    if (!awaitKeyType && curMode == 0) {
                        viewTrack = 1 - viewTrack;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    }
                    break;
                default:
                    if (!awaitKeyType) {
                        FrozenBubble::Instance()->currentState = TitleScreen;
                    }
                    break;
            }
            break;
        case SDL_EVENT_TEXT_INPUT:
            if (newName.size() < 11){
                newName += e->text.text;
                std::string nam = newName + "|";
                nameInput.UpdateText(rend, nam.c_str(), 0);
                showTick = true;
                textTickWait = TEXTANIM_TICKSPEED + 10;
                AudioMixer::Instance()->PlaySFX("typewriter");
            }
            else {
                AudioMixer::Instance()->PlaySFX("stick");
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (e->button.button == SDL_BUTTON_LEFT && !awaitKeyType && curMode == 0) {
                float lx = 0.f, ly = 0.f;
                SDL_RenderCoordinatesFromWindow(rend, e->button.x, e->button.y, &lx, &ly);
                TapScoreTrackTab(lx, ly);
            }
            break;
        case SDL_EVENT_FINGER_DOWN:
            if (!awaitKeyType && curMode == 0) {
                // tfinger is normalized against the window -- scale back up,
                // then let the renderer undo the letterbox, mirroring
                // FrozenBubble::TouchToLogical (frozenbubble.cpp) exactly.
                float lx = 0.f, ly = 0.f;
                SDL_Window *win = SDL_GetRenderWindow(rend);
                int ww = 0, wh = 0;
                if (win) SDL_GetWindowSize(win, &ww, &wh);
                if (ww > 0 && wh > 0) {
                    SDL_RenderCoordinatesFromWindow(rend, e->tfinger.x * (float)ww,
                                                     e->tfinger.y * (float)wh, &lx, &ly);
                } else {
                    lx = e->tfinger.x * 640.f;
                    ly = e->tfinger.y * 480.f;
                }
                TapScoreTrackTab(lx, ly);
            }
            break;
    }
}

void HighscoreManager::TapScoreTrackTab(float lx, float ly) {
    for (int track = 0; track < 2; track++) {
        SDL_Rect box = ScoreTrackTabRect(track);
        if (lx >= box.x && lx < box.x + box.w && ly >= box.y && ly < box.y + box.h) {
            if (track != viewTrack) {
                viewTrack = track;
                AudioMixer::Instance()->PlaySFX("menu_change");
            }
            break;
        }
    }
}
