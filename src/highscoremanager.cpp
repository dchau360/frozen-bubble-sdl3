/*
 * Frozen-Bubble SDL2 C++ Port
 * Copyright (c) 2000-2012 The Frozen-Bubble Team
 * Copyright (c) 2026 Huy Chau
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

#include <fstream>
#include <sstream>
#include <string>

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
std::vector<HighscoreData> levelsetScores;

HighscoreManager *HighscoreManager::ptrInstance = NULL;

HighscoreManager *HighscoreManager::Instance(SDL_Renderer *rend)
{
    if(ptrInstance == NULL)
        ptrInstance = new HighscoreManager(rend);
    return ptrInstance;
}

void HighscoreManager::LoadLevelsetHighscores(const char *path) {
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
                while(std::getline(ss, curChar, ','))
                {
                    if (task == 0) hs.level = stoi(curChar);
                    else if (task == 1) hs.name = curChar;
                    else if (task == 2) hs.time = stof(curChar);
                    else if (task == 3) {
                        hs.picId = stoi(curChar);
                        levelsetScores.push_back(hs);
                    }
                    task++;
                }
                task = 0;
            }
        }
    }
    else {
        SDL_LogError(1, "Could not load highscore levels (%s).", path);
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
                while(std::getline(ss, curChar, ' '))
                {
                    if(curChar.empty()) continue;
                    else if(curChar == "-") line.push_back(-1);
                    else {
                        line.push_back(stoi(curChar));
                    }
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
        SDL_LogError(1, "Could not load highscore levels (%s).", path);
    }
}

void HighscoreManager::AppendToLevels(std::array<std::vector<int>, 10> lvl, int id){
    highscoreLevels[id] = lvl;
    CreateLevelImages();
}

bool HighscoreManager::CheckAndAddScore(int level, float time) {
    // Determine if this score qualifies for top 10 (higher level = better; same level, lower time = better)
    bool qualifies = ((int)levelsetScores.size() < 10);
    for (const auto& s : levelsetScores) {
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
    levelsetScores.push_back(newEntry);

    // Sort: higher level first, then faster time
    std::sort(levelsetScores.begin(), levelsetScores.end(), [](const HighscoreData& a, const HighscoreData& b) {
        if (a.level != b.level) return a.level > b.level;
        return a.time < b.time;
    });
    if (levelsetScores.size() > 10) levelsetScores.resize(10);

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
    panelText.UpdateAlignment(TTF_HORIZONTAL_ALIGN_CENTER);
    nameInput.UpdateAlignment(TTF_HORIZONTAL_ALIGN_CENTER);
    panelText.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});
    nameInput.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});

    panelText.UpdateText(rend, "Congratulations!\n\nYou got a high score!\n\nEnter name:            \n", 0);
    panelText.UpdatePosition({(640/2) - (panelText.Coords()->w / 2), (480/2) - 120});
    nameInput.UpdatePosition({(640/2) - 45 - (panelText.Coords()->w / 2), (480/2) - 25});

    std::string historypath = gameSettings->prefPath + std::string("highlevelshistory");
    std::string levelsetpath = gameSettings->prefPath + std::string("highscores");
    LoadHighscoreLevels(historypath.c_str());
    LoadLevelsetHighscores(levelsetpath.c_str());

    CreateLevelImages();
}

HighscoreManager::~HighscoreManager(){
    levelsetScores.clear();
    TTF_CloseFont(highscoreFont);
}

void HighscoreManager::Dispose(){
    SaveNewHighscores();
    this->~HighscoreManager();
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

void HighscoreManager::SaveNewHighscores() {
    std::string historypath = gameSettings->prefPath + std::string("highlevelshistory");
    std::string levelsetpath = gameSettings->prefPath + std::string("highscores");

    std::ofstream historyFile(historypath);
    for (size_t i = 0; i < highscoreLevels.size(); i++)
    {
        historyFile << levelToData(highscoreLevels[i]);
    }
    historyFile.close();

    std::ofstream levelsetFile(levelsetpath);
    for (size_t i = 0; i < levelsetScores.size(); i++) {
        HighscoreData &a = levelsetScores[i];
        levelsetFile << a.level << "," << a.name << "," << a.time << "," << a.picId << "\n"; 
    }
    levelsetFile.close();
}

void HighscoreManager::CreateLevelImages() {
    SDL_Log("CreateLevelImages: start, highscoreLevels.size=%zu, levelsetScores.size=%zu", highscoreLevels.size(), levelsetScores.size());
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

    SDL_Log("CreateLevelImages: refreshing %zu score texts", levelsetScores.size());
    for (size_t i = 0; i < levelsetScores.size(); i++) {
        SDL_Log("CreateLevelImages: refreshing score %zu", i);
        levelsetScores[i].RefreshTextStatus(rend, highscoreFont);
        SDL_Log("CreateLevelImages: score %zu text refreshed", i);
        SDL_Rect *c = levelsetScores[i].layoutText.Coords();
        if (c) levelsetScores[i].layoutText.UpdatePosition({108 * ((int)i + 1) - c->w/2, (115 * (((int)i + 1) % 6 == 0 ? 2 : 1)) + (70 * (((int)i + 1) % 6 == 0 ? 2 : 1))});
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
        for (size_t i = 0; i < levelsetScores.size(); i++) {
            int sx = 64, sy = 85;
            if (smallBG[i]) { float fw, fh; SDL_GetTextureSize(smallBG[i], &fw, &fh); sx = (int)fw; sy = (int)fh; }
            SDL_Rect bgPos = {85 * (int)(i > 5 ? (i - 5) + 1 : i + 1) + (20 * ((int)i % 6)), (80 * (((int)i + 1) >= 6 ? 1 : 0)) + (80 * (((int)i + 1) >= 6 ? 2 : 1)), sx, sy};
            SDL_Rect framePos = {bgPos.x - 7, bgPos.y - 7, 81, 100};
            { SDL_FRect fr = ToFRect(framePos); SDL_RenderTexture(rend, highscoreFrame, nullptr, &fr); }
            if (smallBG[i]) { SDL_FRect fr = ToFRect(bgPos); SDL_RenderTexture(rend, smallBG[i], nullptr, &fr); }
            { SDL_FRect fr = ToFRect(*levelsetScores[i].layoutText.Coords()); SDL_RenderTexture(rend, levelsetScores[i].layoutText.Texture(), nullptr, &fr); }
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
                        for (int i = (int)levelsetScores.size() - 1; i >= 0; i--) {
                            if (levelsetScores[i].newHighscore) {
                                if (!newName.empty()) {
                                    levelsetScores[i].name = newName;
                                    levelsetScores[i].RefreshTextStatus(rend, highscoreFont);
                                }
                                levelsetScores[i].newHighscore = false;
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
    }
}