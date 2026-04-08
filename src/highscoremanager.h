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

#ifndef HIGHSCOREMANAGER_H
#define HIGHSCOREMANAGER_H

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <vector>
#include <array>
#include <map>

#include "gamesettings.h"
#include "ttftext.h"

#define TEXTANIM_TICKSPEED 2

class HighscoreManager final
{
public:
    void ShowScoreScreen(int ls);
    void ShowNewScorePanel(int mode);
    void RenderPanel(void);
    void RenderScoreScreen(void);
    void HandleInput(SDL_Event *e);
    int lastState;

    void AppendToLevels(std::array<std::vector<int>, 10> lvl, int id);
    bool CheckAndAddScore(int level, float time);  // Returns true if this is a new high score

    HighscoreManager(const HighscoreManager& obj) = delete;
    void Dispose();
    static HighscoreManager* Instance(SDL_Renderer *rend = nullptr);
private:
    int curMode;

    GameSettings *gameSettings;
    SDL_Renderer *rend;

    std::map<int, std::array<std::vector<int>,10>> highscoreLevels;
    void SaveNewHighscores();
    void LoadLevelsetHighscores(const char *path);
    void LoadHighscoreLevels(const char *path);
    void CreateLevelImages();

    SDL_Surface *backgroundSfc, *useBubbles[8];
    SDL_Texture *smallBG[10] = { nullptr }, *highscoresBG, *highscoreFrame, *headerLevelset, *headerMptrain;

    TTF_Font *highscoreFont;
    TTFText panelText, nameInput;

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