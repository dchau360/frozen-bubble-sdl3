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

#include "mainmenu.h"
#include "audiomixer.h"
#include "frozenbubble.h"
#include "localmultiplayer_settings.h"
#include "menulist.h"
#include "transitionmanager.h"
#include "networkclient.h"
#include "platform.h"

#include <SDL3_image/SDL_image.h>
#include <cstring>
#include <cmath>
#include <errno.h>
#include <thread>
#include <mutex>
#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#endif
#include "socket_compat.h"
#ifndef _WIN32
#ifndef __WASM_PORT__
#include <netdb.h>
#endif
#endif
#ifdef __WASM_PORT__
#include <emscripten.h>
#include <stdlib.h>
#endif

#include "mainmenu_internal.h"

static std::string ControllerScancodeName(SDL_Scancode sc) {
    if (!IsVirtualScancode(sc)) return SDL_GetScancodeName(sc);
    int rel = sc - CTRL_SC_BASE;
    int slot = rel / 20;
    int btn  = rel % 20;
    const char* btnNames[] = {"A","B","X","Y","Back","Guide","Start",
                               "LS","RS","LB","RB","DUp","DDown","DLeft","DRight"};
    char buf[32];
    snprintf(buf, sizeof(buf), "Ctrl%d:%s", slot + 1, btn < 15 ? btnNames[btn] : "?");
    return buf;
}

// Returns true if something is already listening on localhost:port

void MainMenu::Render(void) {
    SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), background, nullptr, nullptr);

    // Drop last frame's tap targets before any panel republishes its own. A
    // panel that closed this frame would otherwise leave its rows behind, and a
    // tap on empty space would still land on one.
    panelTapRows.clear();
    panelTapSelection = nullptr;
    panelTapSubSelection = nullptr;

    for (MenuButton &button : buttons) {
        button.Render(renderer);
    }
    BannerRender();
    BlinkRender();
    CandyRender();
    SPPanelRender();
    LocalMPPanelRender();
    OptPanelRender();
    LevelPanelRender();
    KeysPanelRender();
    NetSetupPanelRender();
    NetPanelRender();
}


void MainMenu::BannerRender() {
    bannerCurpos = bannerCurpos != 0 ? bannerCurpos : 670;
    for(size_t i = 0; i < std::size(bannerFormulas); i++) {
        int posX = bannerFormulas[i] - bannerCurpos;
        SDL_Texture *image = i == 0 ? bannerArtwork : (i == 1 ? bannerSound : (i == 2 ? bannerCPU : bannerLevel));
        SDL_Point size = GetSize(image);
        if (posX > bannerMax / 2) posX = bannerFormulas[i] - (bannerCurpos + bannerMax);

        if (posX < BANNER_MAXX && posX + size.x >= 0) {
            SDL_Rect iRect = {-posX, 0, std::min(size.x + posX, BANNER_MAXX - BANNER_MINX), size.y};
            SDL_Rect dRect = {iRect.x < 0 ? BANNER_MAXX - (-posX > -iRect.w ? -posX + iRect.w : 0): BANNER_MINX, BANNER_Y, 
                              iRect.x < 0 ? iRect.w - (-posX > -iRect.w ? posX : 0): iRect.w, size.y};
            { SDL_FRect fSrc = ToFRect(iRect); SDL_FRect fDst = ToFRect(dRect); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), image, &fSrc, &fDst); }
        }
    }

    if(GameSettings::Instance()->gfxLevel() > 2) return;
    if(bannerFU == 0) {
        bannerCurpos++;
        bannerFU = BANNER_SLOWDOWN;
    }
    else bannerFU--;
    if(bannerCurpos >= bannerMax) bannerCurpos = 1;
}


void MainMenu::BlinkRender() {
    if(GameSettings::Instance()->gfxLevel() > 2) return;

    if (waitGreen <= 0) {
        if(blinkGreen > 0) {
            blinkGreen--;
            if(!blinkGreen) {
                waitGreen = BLINK_FRAMES;
                if(ranrange(0, 3) <= 1) blinkGreen = -(5 * BLINK_SLOWDOWN); 
            }
        }
        else if(blinkGreen < 0) {
            blinkGreen++;
            if(!blinkGreen) {
                waitGreen = BLINK_FRAMES;
                blinkGreen = 3 * BLINK_SLOWDOWN; 
            }
        }
        else {
            if(ranrange(0, 200) <= 1) {
                waitGreen = BLINK_FRAMES;
                blinkGreen = 3 * BLINK_SLOWDOWN;
            }
        }
    }
    else {
        waitGreen--;
        { SDL_FRect fr = ToFRect(blink_green_left); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), blinkGreenL, NULL, &fr); }
        { SDL_FRect fr = ToFRect(blink_green_right); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), blinkGreenR, NULL, &fr); }
    }
    
    if(waitPurple <= 0) {
        if(blinkPurple > 0) {
            blinkPurple--;
            if(!blinkPurple) {
                waitPurple = BLINK_FRAMES;
                if(ranrange(0, 3) <= 1) blinkPurple = -(5 * BLINK_SLOWDOWN); 
            }
        }
        else if(blinkPurple < 0) {
            blinkPurple++;
            if(!blinkPurple) {
                waitPurple = BLINK_FRAMES;
                blinkPurple = 3 * BLINK_SLOWDOWN; 
            }
        }
        else {
            if(ranrange(0, 200) <= 1) {
                waitPurple = BLINK_FRAMES;
                blinkPurple = 3 * BLINK_SLOWDOWN;
            }
        }
    }
    else {
        waitPurple--;
        { SDL_FRect fr = ToFRect(blink_purple_left); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), blinkPurpleL, NULL, &fr); }
        { SDL_FRect fr = ToFRect(blink_purple_right); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), blinkPurpleR, NULL, &fr); }
    }

}


void MainMenu::CandyRender() {
    if (!candyInit || GameSettings::Instance()->gfxLevel() > 1) {
        { SDL_FRect fr = ToFRect(fb_logo_rect); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), fbLogo, nullptr, &fr); }
        return;
    }

    if (candyMethod == 0)       rotate_bilinear_(candyModif.sfc, candyOrig.sfc, SDL_sin(candyIndex/40.0)/10.0);
    else if(candyMethod == 1)   flipflop_(candyModif.sfc, candyOrig.sfc, candyIndex);
    else if(candyMethod == 2)   enlighten_(candyModif.sfc, candyOrig.sfc, candyIndex);
    else if(candyMethod == 3)   stretch_(candyModif.sfc, candyOrig.sfc, candyIndex);
    else if(candyMethod == 4)   tilt_(candyModif.sfc, candyOrig.sfc, candyIndex);
    else if(candyMethod == 5)   points_(candyModif.sfc, candyOrig.sfc, logoMask.sfc);
    else if(candyMethod == 6)   waterize_(candyModif.sfc, candyOrig.sfc, candyIndex);
    else if(candyMethod == 7)   brokentv_(candyModif.sfc, candyOrig.sfc, candyIndex);
    else if(candyMethod == 8)   snow_(candyModif.sfc, candyOrig.sfc);

    { SDL_FRect fr = ToFRect(candy_fb_rect); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), candyModif.OutputTexture(), nullptr, &fr); }
    candyIndex++;
}


void MainMenu::SPPanelRender() {
    if (!showingSPPanel) return;

    // activeSPButtons[0] may be null if its asset failed to load; dereferencing
    // it here crashed the client as soon as the panel opened (BUG-044).
    if(overlookSfc == nullptr && activeSPButtons[0] != nullptr) {
        overlookSfc = SDL_CreateSurface(activeSPButtons[0]->w, activeSPButtons[0]->h, SURF_FORMAT);
        overlook_init_(overlookSfc);
    }

    // SP panel needs extra height for SP_OPT items: first item at y=191, each 41px apart, 37px tall
    // For 5 items: last item bottom = 191 + (SP_OPT-1)*41 + 37 = 392 -> need panel bottom >= 400
    SDL_Rect spPanelRct = {(640/2) - (341/2), (480/2) - (320/2), 341, 320};
    { SDL_FRect fr = ToFRect(spPanelRct); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), singlePanelBG, nullptr, &fr); }
    for (int i = 0; i < SP_OPT; i++){
        int w = 0, h = 0;
        { float fw = 0, fh = 0; SDL_GetTextureSize(idleSPButtons[i], &fw, &fh); w = (int)fw; h = (int)fh; }
        SDL_Rect entryRct = {(640/2)-(298/2), ((480/2)-90)+(41 * (i + 1)), 298, 37};
        SDL_Rect subRct = {(640/2)-(298/2), ((480/2)-90)+(41 * (i + 1)), w, h};
        if(i == activeSPIdx) {
            // overlook_ reads orig->format, and overlookSfc is null when the
            // asset that sizes it failed to load. Fall back to the plain
            // highlight instead of dereferencing either (BUG-044).
            if (GameSettings::Instance()->gfxLevel() <= 2
                && overlookSfc != nullptr && activeSPButtons[i] != nullptr) {
                overlook_(overlookSfc, activeSPButtons[i], overlookIndex, spOptions[i].pivot);
                SDL_Rect miniRct = {(640/2)-(298/2), ((480/2)-90)+(41 * (i + 1)), overlookSfc->w, overlookSfc->h};
                SDL_Texture *miniOverlook = SDL_CreateTextureFromSurface(const_cast<SDL_Renderer*>(renderer), overlookSfc);
                
                { SDL_FRect fr = ToFRect(entryRct); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), singleButtonAct, nullptr, &fr); }
                { SDL_FRect fr = ToFRect(miniRct); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), miniOverlook, nullptr, &fr); }
                SDL_DestroyTexture(miniOverlook);

                overlookIndex++;
                if (overlookIndex >= 70) overlookIndex = 0;
            }
            else { SDL_FRect fr = ToFRect(entryRct); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), singleButtonAct, nullptr, &fr); }
        }
        else { SDL_FRect fr = ToFRect(entryRct); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), singleButtonIdle, nullptr, &fr); }
        { SDL_FRect fr = ToFRect(subRct); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), idleSPButtons[i], nullptr, &fr); }
    }

    { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, &fr); };
}


void MainMenu::LocalMPPanelRender() {
    if (!showingLocalMPPanel) return;

    if (runDelay) {
        if (delayTime == 0) SetupNewGame(7);
        else delayTime--;
    }

    int connected = 0;
    { SDL_JoystickID *joys = SDL_GetJoysticks(&connected); SDL_free(joys); }
    // Bots need no controller, so only the human slots count toward the
    // warning -- otherwise adding bots to fix "not enough controllers" would
    // leave the warning up, which reads as though it had not worked.
    const int botCount = ClampLocalBotCount(localMPBotCount, localMPPlayerCount);
    const int humansNeeded = localMPPlayerCount - botCount;
    const bool shortOnControllers = connected < humansNeeded;

    char victoriesText[32];
    if (localMPVictoriesIndex == 0) {
        snprintf(victoriesText, sizeof(victoriesText), "none (unlimited)");
    } else {
        snprintf(victoriesText, sizeof(victoriesText), "%d",
            kVictoriesLimits[localMPVictoriesIndex]);
    }

    char botsText[16];
    snprintf(botsText, sizeof(botsText), botCount == 0 ? "none" : "%d", botCount);

    // Team Mode splits odd slots against even, so which players face which
    // depends on the count -- spell it out rather than naming a fixed pairing.
    char teamModeText[40] = "OFF";
    if (localMPTeamMode) LocalMPTeamSplitLabel(teamModeText, sizeof(teamModeText), localMPPlayerCount);

    SDL_Renderer* rend = const_cast<SDL_Renderer*>(renderer);
    const int startRow = LocalMPStartRow(localMPPlayerCount);

    auto tap = [&](int index, const SDL_Rect& rect, int subIndex, bool splitAdjust, SDL_Keycode key) {
        AddPanelTapRow(index, rect, subIndex, splitAdjust, key);
    };

    BeginPanelTapRows(&localMPMenuIndex);

    char title[64];
    snprintf(title, sizeof(title), "LOCAL MULTIPLAYER  \xe2\x80\x94  %d players", localMPPlayerCount);
    menulist::DrawHeaderBar(rend, panelText, menulist::kHeaderBar, title,
        "Start game!", localMPMenuIndex == startRow, startRow, tap);

    // MainMenuTestAccess reads lastLocalMPPanelText to check row content
    // without a real renderer walking pixels (see mainmenu.h). row() is the
    // single place that both feeds the widget and keeps that dump in sync,
    // so no row can appear in one and not the other.
    lastLocalMPPanelText.clear();
    menulist::List list(menulist::kListFull, localMPMenuIndex);
    auto row = [&](int index, const std::string& label, const std::string& value,
                   bool emphasize = true, bool splitAdjust = false) {
        list.Row(index, label, value, emphasize, splitAdjust);
        lastLocalMPPanelText += label + ": " + value + "\n";
    };

    list.Header("Match rules");
    row(kLocalMPRowPlayers, "Players", std::to_string(localMPPlayerCount), true, true);
    row(kLocalMPRowChain, "Chain reaction", localMPCR ? "ON" : "OFF", localMPCR);
    row(kLocalMPRowCollapse, "Row collapse", localMPNoCompress ? "OFF" : "ON", !localMPNoCompress);
    row(kLocalMPRowMode, "Mode", localMPClearMode ? "Clear" : "Classic", true, true);
    row(kLocalMPRowMalus, "Attack bubbles", localMPDisableMalus ? "OFF" : "ON", !localMPDisableMalus);
    row(kLocalMPRowTeam, "Team mode", teamModeText, localMPTeamMode);
    row(kLocalMPRowVictories, "Victories limit", victoriesText, true, true);

    list.Header("Bots");
    row(kLocalMPRowBots, "Bots", botsText, botCount > 0, true);
    row(kLocalMPRowBotSkill, "Bot skill", LocalMPBotSkillName(localMPBotSkill), true, true);

    list.Header("Per-player");
    for (int pi = 0; pi < localMPPlayerCount && pi < 5; pi++) {
        char label[16];
        snprintf(label, sizeof(label), "P%d aim guide", pi + 1);
        row(LocalMPAimGuideRow(pi), label, localMPAimGuide[pi] ? "ON" : "OFF", localMPAimGuide[pi]);
    }
    for (int pi = 0; pi < localMPPlayerCount && pi < 5; pi++) {
        char label[16], val[8];
        snprintf(label, sizeof(label), "P%d max colors", pi + 1);
        snprintf(val, sizeof(val), "%d", playerColorCounts[pi]);
        row(LocalMPColorsRow(pi, localMPPlayerCount), label, val, true, true);
    }
    // nullptr, not voidPanelBG: that texture is a small wood panel meant for
    // the ~341x280 popups (SPPanelRender, OptPanelRender...), and stretching
    // it to fill this much larger list blows its watermark up into a giant,
    // blurry crest instead of a backdrop. List::End's own solid navy fill is
    // the frame every other converted screen uses.
    list.End(rend, panelText, nullptr, tap);

    // Sidebar: one row per player (human vs bot, and which input device a
    // human is bound to -- reusing the same virtual-scancode test
    // ControllerScancodeName() uses, not a fabricated guess), plus the
    // controller-shortage warning docked at the bottom where it stays next
    // to the slots it's actually talking about, instead of pushing every
    // settings row down like the old single-panel layout did.
    int sy = menulist::DrawSidebarHeader(rend, panelText, menulist::kSidebarFull, "Players");
    GameSettings* gs = GameSettings::Instance();
    PlayerKeys* allKeys[5] = {&gs->player1Keys, &gs->player2Keys, &gs->player3Keys,
                              &gs->player4Keys, &gs->player5Keys};
    const SDL_Rect& sb = menulist::kSidebarFull;
    for (int pi = 0; pi < localMPPlayerCount && pi < 5; pi++) {
        bool isBot = pi >= localMPPlayerCount - botCount;
        SDL_Rect rowRect = {sb.x + 10, sy, sb.w - 20, 26};
        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(rend, 10, 38, 48, isBot ? 90 : 170);
        { SDL_FRect fr = ToFRect(rowRect); SDL_RenderFillRect(rend, &fr); }

        char pname[8];
        snprintf(pname, sizeof(pname), "P%d", pi + 1);
        panelText.UpdateStyle(16, TTF_STYLE_NORMAL);
        panelText.UpdateColor(menulist::kText, menulist::kTextShadow);
        panelText.UpdateText(rend, pname, 0);
        panelText.UpdatePosition({rowRect.x + 6, sy + 4});
        { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(rend, panelText.Texture(), nullptr, &fr); }

        std::string device;
        if (isBot) device = "Bot";
        else device = IsVirtualScancode(allKeys[pi]->fire) ? "Controller" : "Keyboard";
        panelText.UpdateColor(menulist::kMuted, menulist::kTextShadow);
        panelText.UpdateText(rend, device.c_str(), 0);
        panelText.UpdatePosition({rowRect.x + 40, sy + 4});
        { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(rend, panelText.Texture(), nullptr, &fr); }

        sy += 30;
    }

    if (shortOnControllers) {
        char l1[48], l2[48];
        snprintf(l1, sizeof(l1), "%d controller(s) found.", connected);
        snprintf(l2, sizeof(l2), "%d needed to start.", humansNeeded);
        SDL_Rect warnRect = {sb.x + 8, sb.y + sb.h - 66, sb.w - 16, 54};
        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(rend, 120, 30, 30, 130);
        { SDL_FRect fr = ToFRect(warnRect); SDL_RenderFillRect(rend, &fr); }
        SDL_SetRenderDrawColor(rend, menulist::kBad.r, menulist::kBad.g, menulist::kBad.b, 220);
        { SDL_FRect fr = ToFRect(warnRect); SDL_RenderRect(rend, &fr); }
        panelText.UpdateStyle(14, TTF_STYLE_NORMAL);
        panelText.UpdateColor(menulist::kBad, menulist::kTextShadow);
        panelText.UpdateText(rend, l1, 0);
        panelText.UpdatePosition({warnRect.x + 8, warnRect.y + 8});
        { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(rend, panelText.Texture(), nullptr, &fr); }
        panelText.UpdateText(rend, l2, 0);
        panelText.UpdatePosition({warnRect.x + 8, warnRect.y + 30});
        { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(rend, panelText.Texture(), nullptr, &fr); }
    }

    menulist::DrawFooterHint(rend, panelText,
        "UP/DOWN select    LEFT/RIGHT change    ENTER confirm    ESC cancel");
}


void MainMenu::OptPanelRender() {
    if (!showingOptPanel) return;

    if(awaitKp == false && lastOptInput != SDLK_UNKNOWN && !runDelay) { // we got our response
        chainReaction = (lastOptInput == SDLK_Y);

        char pnltxt[256];
        snprintf(pnltxt, sizeof(pnltxt), "Random level\n\n\nEnable chain reaction?\n\n\nY or N?:        %s\n\n\n\n\nEnjoy the game!", SDL_GetKeyName(lastOptInput));
        panelText.UpdateText(const_cast<SDL_Renderer *>(renderer), pnltxt, 0);
        panelText.UpdatePosition({(640/2) - (panelText.Coords()->w / 2), (480/2) - 120});

        delayTime = 120;
        runDelay = true;
    }

    if (runDelay){
        if (delayTime == 0) SetupNewGame(selectedMode);
        else delayTime--;
    }

    { SDL_FRect fr = ToFRect(voidPanelRct); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), voidPanelBG, nullptr, &fr); };
    { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, &fr); };
}


void MainMenu::LevelPanelRender() {
    if (!showingLevelPanel) return;

    char txt[256];
    int maxLevel = 100;
    snprintf(txt, sizeof(txt),
        "Pick start level\n\n"
        "Enter level (1-%d):\n\n"
        "%s_",
        maxLevel, levelInput.c_str());
    panelText.UpdateText(const_cast<SDL_Renderer*>(renderer), txt, 0);
    panelText.UpdatePosition({(640/2) - (panelText.Coords()->w / 2), (480/2) - 80});

    { SDL_FRect fr = ToFRect(voidPanelRct); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), voidPanelBG, nullptr, &fr); };
    { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, &fr); };

    if (runDelay) {
        if (delayTime == 0) {
            showingLevelPanel = false;
            SDL_StopTextInput(SDL_GetKeyboardFocus());
            SetupNewGame(5);  // mode 5 = pick_start_level
        } else {
            delayTime--;
        }
    }
}


void MainMenu::KeysPanelRender() {
    if (!showingKeysPanel) return;

    BeginPanelTapRows(&keyConfigIndex);

#ifdef __ANDROID__
    const bool adsRemoved = AdsRemoved();
#endif

    GameSettings* gs = GameSettings::Instance();
    PlayerKeys* allKeys[5] = {
        &gs->player1Keys, &gs->player2Keys, &gs->player3Keys,
        &gs->player4Keys, &gs->player5Keys
    };
    PlayerKeys& pk = *allKeys[keyConfigPlayer - 1];

    SDL_Renderer* rend = const_cast<SDL_Renderer*>(renderer);

    auto tap = [&](int index, const SDL_Rect& rect, int subIndex, bool splitAdjust, SDL_Keycode key) {
        AddPanelTapRow(index, rect, subIndex, splitAdjust, key);
    };

    char title[64];
    snprintf(title, sizeof(title), "CONTROLS & SETTINGS  (" APP_VERSION ")");
    menulist::DrawHeaderBar(rend, panelText, menulist::kHeaderBar, title, nullptr, false, -1, tap);

    menulist::List list(menulist::kListFull, keyConfigIndex);

    char header[32];
    snprintf(header, sizeof(header), "Player %d keys", keyConfigPlayer);
    list.Header(header);

    struct { int idx; const char* label; SDL_Scancode sc; } keyRows[4] = {
        {kKeyRowLeft,   "Turn left",  pk.left},
        {kKeyRowRight,  "Turn right", pk.right},
        {kKeyRowFire,   "Fire",       pk.fire},
        {kKeyRowCenter, "Center",     pk.center},
    };
    for (auto& row : keyRows) {
        bool awaitingThis = awaitKp && keyConfigIndex == row.idx;
        list.Row(row.idx, row.label,
                 awaitingThis ? "Press a key..." : ControllerScancodeName(row.sc), true);
    }
    list.Row(kKeyRowResetCtrl, "Reset to defaults");

    list.Header("Game");
    char speedVal[8];
    snprintf(speedVal, sizeof(speedVal), "%.1f", gs->speedMultiplier);
    list.Row(kKeyRowSpeed, "Game speed", speedVal, true, true);
    list.Row(kKeyRowSound, "Sound", gs->soundEnabled() ? "ON" : "OFF", gs->soundEnabled());
    list.Row(kKeyRowMouse, "Mouse / touch aim", gs->mouseEnabled ? "ON" : "OFF", gs->mouseEnabled);
#ifndef __WASM_PORT__
    list.Row(kKeyRowFullscreen, "Fullscreen", gs->fullscreenMode() ? "ON" : "OFF", gs->fullscreenMode());
#endif

#ifdef __ANDROID__
    // Ad-removal purchases. These live here rather than behind the old
    // [R]-to-buy hint on the chain-reaction prompt, which no phone, tablet, or
    // TV box could actually trigger -- nothing maps a touch or a controller to
    // R. This panel already handles both, so the rows are reachable everywhere
    // the game runs.
    list.Header("Store");
    if (adsRemoved) {
        // The rows still exist as navigable indices so the enum stays fixed
        // (skippableRow() in mainmenu_input.cpp steps over them), but there is
        // nothing to sell somebody who has already paid, so nothing is drawn
        // for them -- index -1 keeps this line itself unselectable.
        list.Row(-1, "Ads removed -- thank you!", "");
    } else {
        const std::string yearPrice = AdsPrice(0);
        const std::string everPrice = AdsPrice(1);
        // Play's own price string when it has arrived, and an honest
        // placeholder when it has not -- never a hardcoded number, which
        // would be wrong in every currency but one.
        list.Row(kKeyRowRemoveAdsYear, "Remove ads (1 year)",
                 yearPrice.empty() ? "..." : yearPrice + "/yr", true);
        list.Row(kKeyRowRemoveAdsForever, "Remove ads (forever)",
                 everPrice.empty() ? "..." : everPrice, true);
        if (keyConfigIndex == kKeyRowRemoveAdsYear) {
            // Play policy requires auto-renewal be disclosed before purchase,
            // and it is the thing a player most needs to know about the
            // yearly option -- so it is shown on the panel, not buried in
            // the store.
            list.Row(-1, "renews yearly, cancel in Play", "");
        }
    }
#endif

    list.Header("Reset");
    bool resetAllSel = (keyConfigIndex == kKeyRowResetAll);
    list.Row(kKeyRowResetAll, "Reset all settings",
             (resetAllSel && resetAllArmed) ? "Press again to confirm" : "",
             resetAllArmed);

    list.End(rend, panelText, nullptr, tap);

    // Sidebar: which player's bindings are on screen, and a tap target to
    // jump straight to any of the other three -- LEFT/RIGHT already did this
    // from a keyboard, but had no touch equivalent at all before this panel
    // gained a tap system.
    int sy = menulist::DrawSidebarHeader(rend, panelText, menulist::kSidebarFull, "Editing");
    const SDL_Rect& sb = menulist::kSidebarFull;
    for (int p = 1; p <= 4; p++) {
        bool isCurrent = (p == keyConfigPlayer);
        bool tapPending = (keyConfigIndex == kKeyPlayerTapBase + p);
        SDL_Rect rowRect = {sb.x + 10, sy, sb.w - 20, 26};
        if (tapPending) {
            SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(rend, menulist::kSelFill.r, menulist::kSelFill.g,
                                    menulist::kSelFill.b, menulist::kSelFill.a);
            { SDL_FRect fr = ToFRect(rowRect); SDL_RenderFillRect(rend, &fr); }
            SDL_SetRenderDrawColor(rend, menulist::kSelEdge.r, menulist::kSelEdge.g,
                                    menulist::kSelEdge.b, menulist::kSelEdge.a);
            { SDL_FRect fr = ToFRect(rowRect); SDL_RenderRect(rend, &fr); }
        } else {
            SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(rend, 10, 38, 48, isCurrent ? 170 : 90);
            { SDL_FRect fr = ToFRect(rowRect); SDL_RenderFillRect(rend, &fr); }
        }
        char label[16];
        snprintf(label, sizeof(label), "Player %d", p);
        panelText.UpdateStyle(16, isCurrent ? TTF_STYLE_BOLD : TTF_STYLE_NORMAL);
        panelText.UpdateColor(isCurrent ? menulist::kGold : menulist::kText, menulist::kTextShadow);
        panelText.UpdateText(rend, label, 0);
        panelText.UpdatePosition({rowRect.x + 6, sy + 4});
        { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(rend, panelText.Texture(), nullptr, &fr); }

        AddPanelTapRow(kKeyPlayerTapBase + p, rowRect, -1, false,
                       p == 1 ? SDLK_1 : p == 2 ? SDLK_2 : p == 3 ? SDLK_3 : SDLK_4);
        sy += 30;
    }

    int connected = 0;
    { SDL_JoystickID *joys = SDL_GetJoysticks(&connected); SDL_free(joys); }
    char ctrlText[32];
    snprintf(ctrlText, sizeof(ctrlText), "%d controller(s) connected", connected);
    panelText.UpdateStyle(14, TTF_STYLE_NORMAL);
    panelText.UpdateColor(menulist::kMuted, menulist::kTextShadow);
    panelText.UpdateText(rend, ctrlText, 0);
    panelText.UpdatePosition({sb.x + 12, sy + 10});
    { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(rend, panelText.Texture(), nullptr, &fr); }

    menulist::DrawFooterHint(rend, panelText,
        awaitKp ? "Press a button or key..."
                : "UP/DOWN select    ENTER change    1-4 switch player    ESC done");
}


void MainMenu::NetSetupPanelRender() {
    if (!showingNetSetupPanel) return;

    if(awaitKp == false && lastOptInput != SDLK_UNKNOWN && !runDelay) { // we got our response
        chainReaction = (lastOptInput == SDLK_Y);

        char pnltxt[256];
        snprintf(pnltxt, sizeof(pnltxt), "Network game\n\n\nEnable chain reaction?\n\n\nY or N?:        %s\n\n\n\n\nConnecting...", SDL_GetKeyName(lastOptInput));
        panelText.UpdateText(const_cast<SDL_Renderer *>(renderer), pnltxt, 0);
        panelText.UpdatePosition({(640/2) - (panelText.Coords()->w / 2), (480/2) - 120});

        delayTime = 60; // Shorter delay for network games
        runDelay = true;
    }

    if (runDelay){
        if (delayTime == 0) {
            // Open the network lobby after chain reaction is set
            showingNetSetupPanel = false;
            showingNetPanel = true;
            networkInLobby = false;
            networkInputMode = 2; // Start with host/join choice
            awaitKp = false;
            runDelay = false;
            lastOptInput = SDLK_UNKNOWN;
        }
        else delayTime--;
    }

    { SDL_FRect fr = ToFRect(voidPanelRct); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), voidPanelBG, nullptr, &fr); };
    { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, &fr); };
}
