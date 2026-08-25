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
    char warningText[128] = "";
    if (connected < localMPPlayerCount) {
        snprintf(warningText, sizeof(warningText),
            "WARNING: %d controller(s) connected, need %d\n",
            connected, localMPPlayerCount);
    }

    char victoriesText[32];
    if (localMPVictoriesIndex == 0) {
        snprintf(victoriesText, sizeof(victoriesText), "none (unlimited)");
    } else {
        snprintf(victoriesText, sizeof(victoriesText), "%d",
            kVictoriesLimits[localMPVictoriesIndex]);
    }

    // Exactly one blank line separates the header (title + optional warning)
    // from the settings list, whether or not the warning is shown.
    char pnltxt[1024];
    int pos = snprintf(pnltxt, sizeof(pnltxt),
        "Local multiplayer\n"
        "%s"
        "\n"
        "%s Players: %d\n"
        "%s Chain-reaction: %s\n"
        "%s Row collapse: %s\n"
        "%s Mode: %s\n"
        "%s Malus: %s\n"
        "%s Team Mode: %s\n"
        "%s Victories limit: %s\n",
        warningText,
        localMPMenuIndex == 0 ? ">" : " ",
        localMPPlayerCount,
        localMPMenuIndex == 1 ? ">" : " ",
        localMPCR ? "enabled" : "disabled",
        localMPMenuIndex == 2 ? ">" : " ",
        localMPNoCompress ? "disabled" : "enabled",
        localMPMenuIndex == 3 ? ">" : " ",
        localMPClearMode ? "Clear" : "Classic",
        localMPMenuIndex == 4 ? ">" : " ",
        localMPDisableMalus ? "disabled" : "enabled",
        localMPMenuIndex == 5 ? ">" : " ",
        localMPTeamMode ? "ON (P1+P3 vs P2+P4)" : "OFF",
        localMPMenuIndex == 6 ? ">" : " ",
        victoriesText);

    // Per-player rows are collapsed onto one line each (instead of one line
    // per player) so the panel's height stays constant regardless of player
    // count; the selected player's field is bracketed instead of using a
    // leading "> " marker, since a whole line can no longer stand for one item.
    pos += snprintf(pnltxt + pos, sizeof(pnltxt) - pos, "  Aim guide:");
    for (int pi = 0; pi < localMPPlayerCount && pi < 5; pi++) {
        bool sel = localMPMenuIndex == 7 + pi;
        pos += snprintf(pnltxt + pos, sizeof(pnltxt) - pos,
            sel ? " [P%d:%s]" : " P%d:%s",
            pi + 1, localMPAimGuide[pi] ? "on" : "off");
    }
    pos += snprintf(pnltxt + pos, sizeof(pnltxt) - pos, "\n  Max colors:");
    for (int pi = 0; pi < localMPPlayerCount && pi < 5; pi++) {
        bool sel = localMPMenuIndex == 7 + localMPPlayerCount + pi;
        pos += snprintf(pnltxt + pos, sizeof(pnltxt) - pos,
            sel ? " [P%d:%d]" : " P%d:%d",
            pi + 1, playerColorCounts[pi]);
    }
    snprintf(pnltxt + pos, sizeof(pnltxt) - pos,
        "\n%s Start game!\n\n"
        "Each player needs a controller.\n"
        "Use UP/DOWN to select\n"
        "LEFT/RIGHT or ENTER to change\n"
        "Press ESC to cancel",
        localMPMenuIndex == 7 + 2 * localMPPlayerCount ? ">" : " ");

    panelText.UpdateText(const_cast<SDL_Renderer *>(renderer), pnltxt, 0);

    // Size and center the panel around the actual measured text instead of a
    // fixed box, so it always fully contains its content on-screen. padTop
    // must clear the header bar's own height (~18px) — that bar is drawn at
    // ~18% opacity, so text sitting on top of it lets the title-screen
    // background show through the letters instead of a solid backdrop.
    const int padX = 24, padTop = 20, padBottom = 20;
    SDL_Rect textCoords = *panelText.Coords();
    SDL_Rect panelRect = {
        (640 - (textCoords.w + padX * 2)) / 2,
        std::max(10, (480 - (textCoords.h + padTop + padBottom)) / 2),
        textCoords.w + padX * 2,
        std::min(textCoords.h + padTop + padBottom, 480 - 20),
    };
    panelText.UpdatePosition({panelRect.x + padX, panelRect.y + padTop});

    // Slices are drawn top, then mid, then bottom, each overlapping the next
    // by a couple pixels — abutting them exactly leaves a hairline seam
    // where linear texture filtering samples across the crop boundary and
    // shows the background through as a see-through gap.
    SDL_Renderer* rend = const_cast<SDL_Renderer*>(renderer);
    float srcW, srcH;
    SDL_GetTextureSize(voidPanelBG, &srcW, &srcH);
    float topCap = srcH * 0.11f, bottomCap = srcH * 0.14f;
    float overlap = 2.0f;
    SDL_FRect topSrc = {0, 0, srcW, topCap + overlap};
    SDL_FRect topDst = {(float)panelRect.x, (float)panelRect.y, (float)panelRect.w, topCap + overlap};
    SDL_FRect bottomSrc = {0, srcH - bottomCap - overlap, srcW, bottomCap + overlap};
    SDL_FRect bottomDst = {(float)panelRect.x, panelRect.y + panelRect.h - bottomCap - overlap, (float)panelRect.w, bottomCap + overlap};
    SDL_FRect midSrc = {0, topCap, srcW, srcH - topCap - bottomCap};
    SDL_FRect midDst = {(float)panelRect.x, panelRect.y + topCap, (float)panelRect.w, panelRect.h - topCap - bottomCap};
    SDL_RenderTexture(rend, voidPanelBG, &topSrc, &topDst);
    SDL_RenderTexture(rend, voidPanelBG, &midSrc, &midDst);
    SDL_RenderTexture(rend, voidPanelBG, &bottomSrc, &bottomDst);

    { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(rend, panelText.Texture(), nullptr, &fr); };
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

    // Size the panel to the rows this configuration actually shows, rather
    // than the fixed 280px box this used to draw. The content had already
    // outgrown that box before the ad-removal rows existed -- "Reset all
    // settings" and the two help lines rendered outside the wood texture,
    // straight onto the title screen behind it -- and two more rows made it
    // obvious. Measuring keeps it correct as rows come and go across
    // platforms and entitlement states.
    SDL_Renderer* rend = const_cast<SDL_Renderer*>(renderer);
    panelText.UpdateText(rend, "Xy", 0);
    const int lineH = panelText.Coords()->h;
    const int gapH  = 6;

    // Must track the render order below exactly; a row added there without a
    // matching count here just re-creates the overflow this replaced.
    int lines = 8;   // title, LEFT/RIGHT hint, 4 key rows, reset ctrl, speed
    int gaps  = 3;
    lines += 2; gaps += 2;               // sound, mouse
#ifndef __WASM_PORT__
    lines += 1; gaps += 1;               // fullscreen
#endif
#ifdef __ANDROID__
    gaps += 1;
    lines += adsRemoved ? 1 : 2;         // thank-you line, or the two buy rows
    if (!adsRemoved && keyConfigIndex == kKeyRowRemoveAdsYear)
        lines += 1;                      // auto-renewal notice
    if (adsRemoved) gaps += 1;
#endif
    lines += 1; gaps += 1;               // reset all
    lines += 1; gaps += 1;               // "Press button or key..." or its spacer
    lines += 2;                          // UP/DOWN select, ESC when done

    const int padX = 24, padTop = 16, padBottom = 16;
    const int contentH = lines * lineH + gaps * gapH;
    SDL_Rect panelRect = {
        voidPanelRct.x - padX / 2,
        std::max(4, (480 - (contentH + padTop + padBottom)) / 2),
        voidPanelRct.w + padX,
        std::min(contentH + padTop + padBottom, 480 - 8),
    };

    // Same three-slice draw as LocalMPPanelRender: stretching the whole
    // texture to a taller box smears its carved border, so the caps are kept
    // at native height and only the middle is stretched. The 2px overlaps stop
    // linear filtering showing a hairline of background at each seam.
    {
        float srcW, srcH;
        SDL_GetTextureSize(voidPanelBG, &srcW, &srcH);
        const float topCap = srcH * 0.11f, bottomCap = srcH * 0.14f, overlap = 2.0f;
        SDL_FRect topSrc = {0, 0, srcW, topCap + overlap};
        SDL_FRect topDst = {(float)panelRect.x, (float)panelRect.y,
                            (float)panelRect.w, topCap + overlap};
        SDL_FRect bottomSrc = {0, srcH - bottomCap - overlap, srcW, bottomCap + overlap};
        SDL_FRect bottomDst = {(float)panelRect.x,
                               panelRect.y + panelRect.h - bottomCap - overlap,
                               (float)panelRect.w, bottomCap + overlap};
        SDL_FRect midSrc = {0, topCap, srcW, srcH - topCap - bottomCap};
        SDL_FRect midDst = {(float)panelRect.x, panelRect.y + topCap,
                            (float)panelRect.w, panelRect.h - topCap - bottomCap};
        SDL_RenderTexture(rend, voidPanelBG, &topSrc, &topDst);
        SDL_RenderTexture(rend, voidPanelBG, &midSrc, &midDst);
        SDL_RenderTexture(rend, voidPanelBG, &bottomSrc, &bottomDst);
    }

    GameSettings* gs = GameSettings::Instance();
    PlayerKeys* allKeys[5] = {
        &gs->player1Keys, &gs->player2Keys, &gs->player3Keys,
        &gs->player4Keys, &gs->player5Keys
    };
    PlayerKeys& pk = *allKeys[keyConfigPlayer - 1];

    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color yellow = {255, 220, 50, 255};
    SDL_Color black  = {0, 0, 0, 255};

    auto renderLine = [&](const char* txt, SDL_Color fg, int& y) {
        panelText.UpdateColor(fg, black);
        panelText.UpdateText(const_cast<SDL_Renderer*>(renderer), txt, 0);
        panelText.UpdatePosition({(640/2) - (panelText.Coords()->w / 2), y});
        { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, &fr); };
        y += panelText.Coords()->h;
    };

    // Same as renderLine, but records the row's tappable band. The band spans
    // the panel's full width rather than the glyphs': the text is centred and
    // often short ("Sound: ON"), and a hit box that tight is hard to hit with a
    // thumb.
    auto renderRow = [&](int row, const char* txt, SDL_Color fg, int& y,
                         bool splitAdjust = false) {
        int top = y;
        renderLine(txt, fg, y);
        AddPanelTapRow(row, { panelRect.x, top, panelRect.w, y - top },
                       -1, splitAdjust);
    };

    char lineBuf[256];
    int y = panelRect.y + padTop;

    snprintf(lineBuf, sizeof(lineBuf), "Key config  Player %d/4  (" APP_VERSION ")", keyConfigPlayer);
    renderLine(lineBuf, white, y);
    renderLine("LEFT/RIGHT to switch player", white, y);
    y += 6;  // small gap before key rows

    // Rows 0-3: key bindings
    struct { int idx; const char* label; std::string val; } rows[4] = {
        {0, "turn left?  ", ControllerScancodeName(pk.left)},
        {1, "turn right? ", ControllerScancodeName(pk.right)},
        {2, "fire?       ", ControllerScancodeName(pk.fire)},
        {3, "center?     ", ControllerScancodeName(pk.center)},
    };
    for (auto& row : rows) {
        bool sel = (keyConfigIndex == row.idx);
        if (sel && awaitKp)
            snprintf(lineBuf, sizeof(lineBuf), "[ %s<-- ]", row.label);
        else if (sel)
            snprintf(lineBuf, sizeof(lineBuf), "[ %s%s ]", row.label, row.val.c_str());
        else
            snprintf(lineBuf, sizeof(lineBuf), "  %s%s  ", row.label, row.val.c_str());
        renderRow(row.idx, lineBuf, sel ? yellow : white, y);
    }

    y += 6;

    // Row 4: Reset ctrl defaults
    bool resetSel = (keyConfigIndex == kKeyRowResetCtrl);
    renderRow(kKeyRowResetCtrl, resetSel ? "[ Reset ctrl defaults ]" : "  Reset ctrl defaults  ", resetSel ? yellow : white, y);

    y += 6;

    // Row 5: Game Speed
    float spd = gs->speedMultiplier;
    bool spdSel = (keyConfigIndex == kKeyRowSpeed);
    // The arrows read as "adjustable sideways" to a keyboard player and as
    // "tap this end" to a touch one, which the old "(L/R adjust)" did not: on a
    // phone there are no L and R keys to press.
    snprintf(lineBuf, sizeof(lineBuf),
        spdSel ? "[ <  Game Speed: %.1f  > ]" : "  Game Speed: %.1f  ", spd);
    renderRow(kKeyRowSpeed, lineBuf, spdSel ? yellow : white, y, true);

    y += 6;

    // Row 6: Sound toggle
    bool sndSel = (keyConfigIndex == kKeyRowSound);
    const char* sndState = gs->soundEnabled() ? "ON" : "OFF";
    snprintf(lineBuf, sizeof(lineBuf), sndSel ? "[ Sound: %s ]" : "  Sound: %s  ", sndState);
    renderRow(kKeyRowSound, lineBuf, sndSel ? yellow : white, y);

    y += 6;

    // Row 7: Mouse/touch toggle
    bool mouseSel = (keyConfigIndex == kKeyRowMouse);
    const char* mouseState = gs->mouseEnabled ? "ON" : "OFF";
    snprintf(lineBuf, sizeof(lineBuf), mouseSel ? "[ Mouse/Touch: %s ]" : "  Mouse/Touch: %s  ", mouseState);
    renderRow(kKeyRowMouse, lineBuf, mouseSel ? yellow : white, y);

#ifndef __WASM_PORT__
    y += 6;

    // Row 8: Fullscreen toggle
    bool fsSel = (keyConfigIndex == kKeyRowFullscreen);
    const char* fsState = gs->fullscreenMode() ? "ON" : "OFF";
    snprintf(lineBuf, sizeof(lineBuf), fsSel ? "[ Fullscreen: %s ]" : "  Fullscreen: %s  ", fsState);
    renderRow(kKeyRowFullscreen, lineBuf, fsSel ? yellow : white, y);
#endif

#ifdef __ANDROID__
    // Ad-removal purchases. These live here rather than behind the old
    // [R]-to-buy hint on the chain-reaction prompt, which no phone, tablet, or
    // TV box could actually trigger -- nothing maps a touch or a controller to
    // R. This panel already handles both, so the rows are reachable everywhere
    // the game runs.
    y += 6;
    {
        const bool removed = adsRemoved;
        if (removed) {
            // One line instead of two buy rows. The rows still exist as
            // navigable indices so the enum stays fixed, but there is nothing
            // to sell somebody who has already paid.
            renderLine("  Ads removed -- thank you!  ", white, y);
            y += 6;
        } else {
            const std::string yearPrice = AdsPrice(0);
            const std::string everPrice = AdsPrice(1);

            bool yearSel = (keyConfigIndex == kKeyRowRemoveAdsYear);
            // Play's own price string when it has arrived, and an honest
            // placeholder when it has not -- never a hardcoded number, which
            // would be wrong in every currency but one.
            snprintf(lineBuf, sizeof(lineBuf),
                     yearSel ? "[ Remove ads 1 year: %s/yr ]" : "  Remove ads 1 year: %s/yr  ",
                     yearPrice.empty() ? "..." : yearPrice.c_str());
            renderRow(kKeyRowRemoveAdsYear, lineBuf, yearSel ? yellow : white, y);

            bool everSel = (keyConfigIndex == kKeyRowRemoveAdsForever);
            snprintf(lineBuf, sizeof(lineBuf),
                     everSel ? "[ Remove ads forever: %s ]" : "  Remove ads forever: %s  ",
                     everPrice.empty() ? "..." : everPrice.c_str());
            renderRow(kKeyRowRemoveAdsForever, lineBuf, everSel ? yellow : white, y);

            // Play policy requires auto-renewal be disclosed before purchase,
            // and it is the thing a player most needs to know about the yearly
            // option -- so it is shown on the panel, not buried in the store.
            if (yearSel) renderLine("  renews yearly, cancel in Play  ", white, y);
        }
    }
#endif

    y += 6;

    // Last row: Reset all settings. Armed by one press and committed by a
    // second, so the label has to say which state it is in -- otherwise the
    // first press looks like it did nothing at all.
    bool resetAllSel = (keyConfigIndex == kKeyRowResetAll);
    if (resetAllSel && resetAllArmed)
        renderRow(kKeyRowResetAll, "[ Press again to confirm ]", yellow, y);
    else
        renderRow(kKeyRowResetAll, resetAllSel ? "[ Reset all settings ]" : "  Reset all settings  ",
                  resetAllSel ? yellow : white, y);

    y += 6;

    if (awaitKp)
        renderLine("Press button or key...", yellow, y);
    else
        y += panelText.Coords()->h;  // keep spacing consistent

    renderLine("UP/DOWN select, ENTER change", white, y);
    renderLine("ESC when done", white, y);
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
