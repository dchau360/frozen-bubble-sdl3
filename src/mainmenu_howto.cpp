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

// How to play: a full-screen page reached from the 1-player menu
// (kSPRowHowTo) and from CONTROLS & SETTINGS (kKeyRowHowTo). Its centre is
// a picture of the bottom of the 1-player board -- the game's own background,
// cannon and bubbles, drawn live so it matches the bubble set in use -- with
// the loaded bubble, the next bubble and the Swap pocket each labelled with
// this player's own control (controlhints.h), then the three steps of a swap
// drawn small. Drawing and one Done button only: it changes no setting.

#include "mainmenu_internal.h"
#include "menulist.h"
#include "modernui.h"
#include "controlhints.h"

#include <SDL3_image/SDL_image.h>
#include <deque>

using namespace modernui;

namespace {

constexpr SDL_Rect kCard = {20, 12, 600, 428};
constexpr SDL_Rect kDone = {512, 400, 92, 28};

// The part of back_one_player.png the picture shows (the launcher, the next
// bubble's ring and the pocket), and how much it is enlarged.
constexpr SDL_Rect kSrc = {232, 338, 180, 142};
constexpr float kScale = 1.3f;
constexpr int kPicX = 203, kPicY = 54;

// Board rects from the 1-player layout (BubbleGame's case 1).
constexpr SDL_Rect kShooter = {270, 357, 100, 100};
constexpr SDL_Rect kLaunch = {304, 391, 32, 32};
constexpr SDL_Rect kNext = {304, 440, 32, 32};
constexpr SDL_Rect kOnTop = {301, 437, 39, 39};
constexpr SDL_Rect kPocket = {258, 440, 32, 32};   // BubbleGame::PocketRect

SDL_FRect Map(SDL_Rect r) {
    return {kPicX + (r.x - kSrc.x) * kScale, kPicY + (r.y - kSrc.y) * kScale, r.w * kScale, r.h * kScale};
}

// bubble-N.png, 0-based as in BubbleGame: 6 red, 2 blue, 3 green, 4 yellow.
constexpr int kRed = 6, kBlue = 2, kGreen = 3, kYellow = 4;
constexpr int kColors[] = {kRed, kBlue, kGreen, kYellow};

}  // namespace

struct MainMenu::HowToText {
    TTF_Font *title = nullptr, *label = nullptr, *body = nullptr, *small = nullptr, *tag = nullptr;
    TTF_Font *chevron = nullptr;   // a › (not in Baloo)
    std::deque<TTFText> pool;
    size_t used = 0;
    TTFText footer;
    SDL_Texture *back = nullptr, *shooter = nullptr, *onTop = nullptr;
    SDL_Texture* bubble[BUBBLE_STYLES] = {};
    bool modernBubbles = false, colorBlind = false;

    TTFText& Take(TTF_Font* f) {
        if (used == pool.size()) pool.emplace_back();
        TTFText& t = pool[used++];
        t.LoadFont(f);
        return t;
    }
};

void MainMenu::FreeHowTo() {
    if (!howTo) return;
    HowToText* h = howTo;
    howTo = nullptr;
    for (SDL_Texture* t : {h->back, h->shooter, h->onTop}) if (t) SDL_DestroyTexture(t);
    for (SDL_Texture* t : h->bubble) if (t) SDL_DestroyTexture(t);
    TTF_Font* fonts[] = {h->title, h->label, h->body, h->small, h->tag, h->chevron};
    delete h;  // the TTFTexts only borrow the fonts, so they go first
    for (TTF_Font* f : fonts) if (f) TTF_CloseFont(f);
}

void MainMenu::OpenHowTo() {
    showingHowTo = true;
    howToSelection = 0;
    // Built fresh each time, so the bubbles follow the Bubbles setting.
    FreeHowTo();
}

void MainMenu::CloseHowTo() {
    showingHowTo = false;
    FreeHowTo();
    PlayMenuSFX("cancel");
}

bool MainMenu::HowToKey(SDL_Event* e) {
    if (!showingHowTo) return false;
    if (e->type != SDL_EVENT_KEY_DOWN) return e->type == SDL_EVENT_TEXT_INPUT;
    switch (e->key.key) {
    case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_ESCAPE: case SDLK_AC_BACK: case SDLK_BACKSPACE:
        CloseHowTo();
        break;
    default:
        break;
    }
    return true;   // nothing behind the page acts while it is open
}

void MainMenu::HowToRender() {
    SDL_Renderer* rend = const_cast<SDL_Renderer*>(renderer);
    GameSettings* gs = GameSettings::Instance();
    if (!howTo) {
        howTo = new HowToText;
        HowToText& h = *howTo;
        const std::string display = ASSET("/gfx/Baloo2-ExtraBold.ttf");
        const std::string body = ASSET("/gfx/DroidSans.ttf");
        h.title = TTF_OpenFont(display.c_str(), 26);
        h.label = TTF_OpenFont(display.c_str(), 14);
        h.body = TTF_OpenFont(body.c_str(), 12);
        h.small = TTF_OpenFont(body.c_str(), 10);
        h.tag = TTF_OpenFont(body.c_str(), 9);
        h.chevron = TTF_OpenFont(body.c_str(), 26);
        if (h.chevron) TTF_SetFontStyle(h.chevron, TTF_STYLE_BOLD);
        if (h.small) TTF_SetFontStyle(h.small, TTF_STYLE_BOLD);
        if (h.tag) TTF_SetFontStyle(h.tag, TTF_STYLE_BOLD);
        h.footer.LoadFont(body.c_str(), 14);
        h.back = IMG_LoadTexture(rend, ASSET("/gfx/back_one_player.png").c_str());
        h.shooter = IMG_LoadTexture(rend, ASSET("/gfx/shooter.png").c_str());
        h.onTop = IMG_LoadTexture(rend, ASSET("/gfx/on_top_next.png").c_str());
        const bool modern = gs->modernBubbles(), blind = gs->colorBlind();
        for (int c : kColors) {
            char rel[96];
            snprintf(rel, sizeof(rel), "/gfx/balls/%sbubble-%s%d.%s", modern ? "modern/" : "",
                     blind ? "colourblind-" : "", c + 1, modern ? "png" : "gif");
            h.bubble[c] = IMG_LoadTexture(rend, ASSET(rel).c_str());
        }
    }
    HowToText& h = *howTo;
    h.used = 0;

    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(rend, 5, 10, 22, 170);
    SDL_RenderFillRect(rend, nullptr);
    DrawCard(rend, kCard, 18);
    DrawTextLine(rend, h.Take(h.title), "HOW TO PLAY", kValue, kTextShadow, menulist::kBackBtn.x + menulist::kBackBtn.w + 16, kCard.y + 26);
    DrawTextLine(rend, h.Take(h.small), "ESC  BACK", kLabel, kNoShadow, kCard.x + kCard.w - 20, kCard.y + 26, 2);

    // ---- The picture -------------------------------------------------------
    const SDL_FRect pic = {(float)kPicX, (float)kPicY, kSrc.w * kScale, kSrc.h * kScale};
    if (h.back) { SDL_FRect src = ToFRect(kSrc); SDL_RenderTexture(rend, h.back, &src, &pic); }
    auto ball = [&](int c, SDL_FRect r) { if (h.bubble[c]) SDL_RenderTexture(rend, h.bubble[c], nullptr, &r); };
    const SDL_FRect launch = Map(kLaunch), next = Map(kNext), pocket = Map(kPocket);
    ball(kBlue, next);
    if (h.onTop) { SDL_FRect r = Map(kOnTop); SDL_RenderTexture(rend, h.onTop, nullptr, &r); }
    DrawPocketWell(rend, h.Take(h.tag), pocket, h.bubble[kGreen], true);
    ball(kRed, launch);
    if (h.shooter) { SDL_FRect r = Map(kShooter); SDL_RenderTexture(rend, h.shooter, nullptr, &r); }
    ball(kRed, launch);
    StrokeRoundRect(rend, {pic.x - 1, pic.y - 1, pic.w + 2, pic.h + 2}, 10, 2, kEdge);

    // A label beside the picture with a line to the bubble it names.
    const bool mouse = gs->mouseEnabled;
    auto callout = [&](bool left, float bubbleCy, float bubbleEdgeX, const char* name, SDL_Color c,
                       const std::string& line1, const char* line2) {
        const int textX = left ? kPicX - 14 : (int)(pic.x + pic.w) + 14;
        const int align = left ? 2 : 0;
        const int cy = (int)bubbleCy;
        DrawTextLine(rend, h.Take(h.label), name, c, kTextShadow, textX, cy - 15, align);
        DrawTextLine(rend, h.Take(h.body), line1.c_str(), kValue, kNoShadow, textX, cy + 2, align);
        if (line2) DrawTextLine(rend, h.Take(h.small), line2, kLabel, kNoShadow, textX, cy + 17, align);
        const float lineFrom = left ? textX + 6.f : textX - 6.f;
        SDL_SetRenderDrawColor(rend, c.r, c.g, c.b, 230);
        SDL_RenderLine(rend, lineFrom, bubbleCy, bubbleEdgeX, bubbleCy);
        SDL_RenderLine(rend, lineFrom, bubbleCy + 1, bubbleEdgeX, bubbleCy + 1);
        const SDL_FRect dot = {bubbleEdgeX - 3, bubbleCy - 2.5f, 6, 6};
        FillRoundRect(rend, dot, 3, c);
    };
    const std::string fire = controlhints::FireControl(mouse);
    const std::string swap = controlhints::SwapControl(mouse);
    callout(false, launch.y + launch.h / 2, launch.x + launch.w + 2, "LOADED", kIce,
            fire.empty() ? "Fire shoots it" : "Fire: " + fire, "your next shot");
    callout(false, next.y + next.h / 2, next.x + next.w + 4, "NEXT", kFrost,
            "Comes up after each shot", nullptr);
    callout(true, pocket.y + pocket.h / 2, pocket.x - 4, "POCKET", kGold,
            swap.empty() ? "Swap" : "Swap: " + swap, "keeps a bubble for later");

    // ---- A swap, in three steps ---------------------------------------------
    const int stripY = 282;
    DrawTextLine(rend, h.Take(h.small), "HOW A SWAP GOES", kLabel, kNoShadow, kCard.x + 24, stripY - 22);
    SDL_SetRenderDrawColor(rend, kLine.r, kLine.g, kLine.b, kLine.a);
    { SDL_FRect divider = {(float)kCard.x + 24, (float)stripY - 14, (float)kCard.w - 48, 1}; SDL_RenderFillRect(rend, &divider); }
    struct Step { int loaded, next, pocket, shot; const char* caption; };
    const Step steps[3] = {
        {kRed, kBlue, -1, -1, "Start"},
        {kBlue, kGreen, kRed, -1, "Swap: the red one is pocketed"},
        {kGreen, kYellow, kBlue, kRed, "Swap again: the red one is shot"},
    };
    constexpr float b = 22;   // bubble size in the strip
    for (int i = 0; i < 3; i++) {
        const float x = kCard.x + 70 + i * 190.f;   // the pocket's left edge
        const SDL_FRect pk = {x, stripY + 30.f, b, b}, nx = {x + 32, stripY + 30.f, b, b},
                        ld = {x + 32, stripY + 4.f, b, b};
        DrawPocketWell(rend, h.Take(h.tag), pk, steps[i].pocket >= 0 ? h.bubble[steps[i].pocket] : nullptr,
                       steps[i].pocket >= 0);
        ball(steps[i].next, nx);
        StrokeRoundRect(rend, {nx.x - 2, nx.y - 2, nx.w + 4, nx.h + 4}, nx.w / 2 + 2, 2, {0, 0, 0, 200});
        ball(steps[i].loaded, ld);
        if (steps[i].shot >= 0) {
            // The shot leaving the launcher, with a short trail.
            SDL_FRect up = {ld.x + 26, ld.y - 6, b, b};
            SDL_SetRenderDrawColor(rend, kIce.r, kIce.g, kIce.b, 200);
            SDL_RenderLine(rend, up.x + b / 2, up.y + b + 2, up.x + b / 2, up.y + b + 14);
            ball(steps[i].shot, up);
        }
        DrawTextLine(rend, h.Take(h.small), steps[i].caption, i ? kFrost : kLabel, kNoShadow,
                     (int)(x + 27), stripY + 66, 1);
        if (i < 2) DrawTextLine(rend, h.Take(h.chevron), "\xE2\x80\xBA", kIce, kNoShadow, (int)(x + 125), stripY + 28, 1);
    }

    // ---- The rules, in words ------------------------------------------------
    const char* rules[] = {
        "Fire shoots the loaded bubble, as always. The pocket empties at every new level.",
        "Swap with the pocket empty puts the loaded bubble in it; the next one comes up.",
        "Swap with a bubble pocketed shoots that one; the loaded bubble takes its place.",
    };
    int ry = stripY + 86;
    for (const char* r : rules) {
        DrawTextLine(rend, h.Take(h.body), r, kFrost, kNoShadow, kCard.x + 24, ry);
        ry += 17;
    }
    (void)ry;

    BeginPanelTapRows(&howToSelection);
    DrawPill(rend, h.Take(h.label), "Done", kDone, true, true);
    AddPanelTapRow(0, kDone, -1, false, SDLK_RETURN);

    menulist::DrawFooterHint(rend, h.footer, "ENTER or ESC  back");
}
