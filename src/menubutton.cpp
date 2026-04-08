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

#include "menubutton.h"
#include "sdl3_compat.h"
#include "frozenbubble.h"
#include "gamesettings.h"
#include "mainmenu.h"
#include "platform.h"

MenuButton::MenuButton(uint32_t x, uint32_t y, const std::string &name, const SDL_Renderer *renderer, const std::string icontag, const int sheetlen)
    : isActive(false)
{
    std::string pathBase = ASSET("/gfx/menu/anims/") + icontag + "_";
    std::string pathExt = ".png";

    if (name != "graphics") {
        for (int i = 1; i < sheetlen + 1; i++)
        {
            std::string fileName(pathBase);
            char fileIndex[16];

            snprintf(fileIndex, sizeof(fileIndex), "%04d", i);
            fileName += fileIndex;
            fileName += pathExt;
            icons.push_back(IMG_LoadTexture(const_cast<SDL_Renderer*>(renderer), fileName.c_str()));
        }

        fixedFrame = curFrame = 0;
        sheetLen = sheetlen;
    }
    else { //graphics mode hot reload quirk
        for (int i = 0; i <= 2; i++)
        {
            pathBase = ASSET("/gfx/menu/anims/gfx-l") + std::to_string(i + 1) + "_";
            int max = sheetlen;
            if(i == 2) max = 1;  // gfx-l3 is a single static frame (highest quality)
            for (int j = 0; j < max; j++)
            {
                sheetLen++;
                std::string fileName = pathBase;
                char fileIndex[16];

                snprintf(fileIndex, sizeof(fileIndex), "%04d", j + 1);
                fileName += fileIndex;
                fileName += pathExt;
                icons.push_back(IMG_LoadTexture(const_cast<SDL_Renderer*>(renderer), fileName.c_str()));
            }
        }

        fixedFrame = (30 * (GameSettings::Instance()->gfxLevel() - 1));
        curFrame = fixedFrame * 2;
        sheetLen = 60;
    }

    std::string backgroundActive_path = ASSET("/gfx/menu/txt_") + name + "_over.png";
    backgroundActive= IMG_LoadTexture(const_cast<SDL_Renderer*>(renderer), backgroundActive_path.c_str());
    std::string background_path = ASSET("/gfx/menu/txt_") + name + "_off.png";
    background= IMG_LoadTexture(const_cast<SDL_Renderer*>(renderer), background_path.c_str());
    rect.x = x;
    rect.y = y;
    rect.w = 202;
    rect.h = 46;
    icon_rect.x = 248;
    icon_rect.y = y + 8;
    icon_rect.w = 40;
    icon_rect.h = 30;

    buttonName = name;
}

MenuButton::MenuButton(MenuButton &&src) noexcept
    : isActive(std::move(src.isActive)),
    sheetLen(std::move(src.sheetLen)),
    curFrame(std::move(src.curFrame)),
    fixedFrame(std::move(src.fixedFrame)),
    buttonName(std::move(src.buttonName)),
    icons(std::move(src.icons)),
    icon_rect(std::move(src.icon_rect)),
    backgroundActive(std::exchange(src.backgroundActive, nullptr)),
    background(std::exchange(src.background, nullptr)),
    rect(std::move(src.rect))
{
}

MenuButton::~MenuButton()
{
    for (long unsigned int i = 0; i < icons.size(); i++) SDL_DestroyTexture(icons[i]);
    icons.clear();
    SDL_DestroyTexture(background);
    SDL_DestroyTexture(backgroundActive);
}

void MenuButton::Render(const SDL_Renderer *renderer)
{
    int gfxLvl = GameSettings::Instance()->gfxLevel();

    if(buttonName == "graphics") {
        if (gfxLvl != 1 && sheetLen != 60) {
            sheetLen = 60;
            curFrame = (30 * (gfxLvl - 1)) * 2;
            fixedFrame = curFrame / 2;

        }
        else if (gfxLvl == 1 && sheetLen != 30) 
        {
            fixedFrame = (curFrame = curFrame / 2) / 2;
            sheetLen = 30;
        }
    }

    if(isActive){
        if(gfxLvl != 3) // animate if gfx mode isn't 3
        {
            curFrame++;
            fixedFrame = curFrame / 2;
            if(fixedFrame >= sheetLen ) {
                if(buttonName != "graphics") fixedFrame = curFrame = 0;
                else { // for the gfx icon, update in a different way depending on the amount of frames
                    curFrame = (30 * (gfxLvl - 1)) * 2;
                    fixedFrame = curFrame / 2;
                }
            }
        }
        if(buttonName == "graphics" && gfxLvl == 3) fixedFrame = 60;
        if(fixedFrame < (int)icons.size() && icons[fixedFrame])
            SDL_SetTextureAlphaMod(icons[fixedFrame], 255);
    }
    else {
        if(fixedFrame < (int)icons.size() && icons[fixedFrame])
            SDL_SetTextureAlphaMod(icons[fixedFrame], 100);
    }
    { SDL_FRect fr = ToFRect(rect); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), isActive?backgroundActive:background, nullptr, &fr); }
    if(fixedFrame < (int)icons.size() && icons[fixedFrame])
    { SDL_FRect fr = ToFRect(icon_rect); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), icons[fixedFrame], nullptr, &fr); }
}

void MenuButton::Pressed(void *parent)
{
    //oh boy
    if(buttonName == "graphics") GameSettings::Instance()->SetValue("GFX:Quality", "");
    else if(buttonName == "1pgame") ((MainMenu *)parent)->ShowPanel(0);
    else if(buttonName == "2pgame") ((MainMenu *)parent)->ShowPanel(2); // Local 2-player (same keyboard)
    else if(buttonName == "langame") ((MainMenu *)parent)->ShowPanel(3); // LAN: UDP broadcast discovery
    else if(buttonName == "netgame") ((MainMenu *)parent)->ShowPanel(5); // Net: manual server entry
    else if(buttonName == "keys") ((MainMenu *)parent)->ShowPanel(4); // Keys configuration
    else if(buttonName == "highscores") HighscoreManager::Instance()->ShowScoreScreen(0);
}

void MenuButton::Activate()
{
    isActive = true;
}

void MenuButton::Deactivate()
{
    isActive = false;
}
