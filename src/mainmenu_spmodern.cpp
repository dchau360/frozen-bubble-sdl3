#include "mainmenu.h"
#include "menulist.h"
#include "modernui.h"
#include "sdl3_compat.h"
#include "gamesettings.h"
#include "platform.h"
#include <SDL3_image/SDL_image.h>
#include <string>

// The 1-player menu in the Modern menu style (MENU_THEME_MODERN): a card in
// the in-game theme's colours with a candy bubble and a line of explanation
// on each game mode, and the settings rows drawn as switches and values.
// Drawing only: the rows, their indices (activeSPIdx, kSPRow*), ENTER, ESC
// and the tap rows are the wood panel's, so press() and the tap handling
// cannot tell the two apart.
//
// Input parity (CLAUDE.md): UP/DOWN move, ENTER / A activates, LEFT/RIGHT flip
// a setting (MenuLeftRightKey), ESC / B goes back; every row is a tap row; the
// focused row has the ice edge; the footer spells it out.

namespace {
using namespace modernui;

constexpr int kBigRows = 5, kSlimRows = SP_OPT - kBigRows;
constexpr SDL_Rect kCard = {150, 8, 340, 436};
constexpr int kRowX = 166, kRowW = 308;
constexpr int kBigTop = 52, kBigPitch = 45, kBigH = 40;
constexpr int kSlimTop = 300, kSlimPitch = 27, kSlimH = 25;


struct Mode { const char* label; const char* sub; int ball; };
// The same five rows as kSPLabel, each with a line saying what it does and
// the candy bubble (share/gfx/balls/modern/bubble-N.png) drawn beside it.
constexpr Mode kModes[kBigRows] = {
    {"PLAY DEFAULT LEVELSET", "100 levels from level 1. Counts for online highscores.", 7},
    {"PICK LEVELSET AND LEVEL", "Any set, any level you have reached.", 3},
    {"RANDOM LEVELS", "A new board every level, endless.", 4},
    {"MULTIPLAYER TRAINING", "Practise against the clock, attack bubbles on.", 5},
    {"HOW TO PLAY", "Aim, shoot, and the Swap pocket.", 6},
};
}  // namespace

// Each line on screen has its own TTFText, so only a line whose text or
// colour changes renders again.
struct MainMenu::SPModernText {
    TTF_Font *title = nullptr, *label = nullptr, *value = nullptr;
    TTF_Font *body = nullptr, *small = nullptr, *slim = nullptr, *chevron = nullptr;
    TTFText titleText, back, options, optionHint, footer;
    TTFText modeLabel[kBigRows], modeSub[kBigRows];
    TTFText slimLabel[kSlimRows], slimValue[kSlimRows], chevL, chevR;
    SDL_Texture* ball[kBigRows] = {};
};

void MainMenu::FreeSPModern() {
    if (!spModern) return;
    for (SDL_Texture* t : spModern->ball) SDL_DestroyTexture(t);
    SPModernText* s = spModern;
    spModern = nullptr;
    TTF_Font* fonts[] = {s->title, s->label, s->value, s->body, s->small, s->slim, s->chevron};
    delete s;  // the TTFTexts only borrow the fonts, so they go first
    for (TTF_Font* f : fonts) if (f) TTF_CloseFont(f);
}


void MainMenu::SPPanelRenderModern() {
    SDL_Renderer* rend = const_cast<SDL_Renderer*>(renderer);
    if (!spModern) {
        spModern = new SPModernText;
        SPModernText& s = *spModern;
        const std::string display = ASSET("/gfx/Baloo2-ExtraBold.ttf");
        const std::string body = ASSET("/gfx/DroidSans.ttf");
        s.title = TTF_OpenFont(display.c_str(), 26);
        s.label = TTF_OpenFont(display.c_str(), 15);
        s.value = TTF_OpenFont(display.c_str(), 13);
        s.body = TTF_OpenFont(body.c_str(), 10);
        s.small = TTF_OpenFont(body.c_str(), 9);
        s.slim = TTF_OpenFont(body.c_str(), 13);
        s.chevron = TTF_OpenFont(body.c_str(), 16);
        if (s.small) TTF_SetFontStyle(s.small, TTF_STYLE_BOLD);
        if (s.chevron) TTF_SetFontStyle(s.chevron, TTF_STYLE_BOLD);
        s.titleText.LoadFont(s.title);
        s.back.LoadFont(s.small);
        s.options.LoadFont(s.small);
        s.optionHint.LoadFont(s.body);
        s.footer.LoadFont(body.c_str(), 14);
        for (int i = 0; i < kBigRows; i++) {
            s.modeLabel[i].LoadFont(s.label);
            s.modeSub[i].LoadFont(s.body);
            s.ball[i] = IMG_LoadTexture(rend,
                ASSET(("/gfx/balls/modern/bubble-" + std::to_string(kModes[i].ball) + ".png").c_str()).c_str());
        }
        for (int t = 0; t < kSlimRows; t++) {
            s.slimLabel[t].LoadFont(s.slim);
            // The account row's value is a › (not in Baloo).
            s.slimValue[t].LoadFont(t == kSlimRows - 1 ? s.chevron : s.value);
        }
        s.chevL.LoadFont(s.chevron);
        s.chevR.LoadFont(s.chevron);
    }
    SPModernText& s = *spModern;

    // The title screen stays visible behind, dimmed, as in the in-game cards.
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(rend, 5, 10, 22, 140);
    SDL_RenderFillRect(rend, nullptr);

    const SDL_FRect card = ToFRect(kCard);
    FillRoundRect(rend, {card.x, card.y + 4, card.w, card.h}, 18, {0, 0, 0, 77});
    FillRoundRect(rend, card, 18, {12, 24, 48, 242});
    StrokeRoundRect(rend, {card.x + 1, card.y + 1, card.w - 2, card.h - 2}, 17, 2, {127, 214, 255, 128});
    DrawTextLine(rend, s.titleText, "1 PLAYER", kValue, kTextShadow, kCard.x + 20, kCard.y + 26);
    DrawTextLine(rend, s.back, "ESC  BACK", kLabel, kNoShadow, kCard.x + kCard.w - 20, kCard.y + 26, 2);

    BeginPanelTapRows(&activeSPIdx);

    for (int i = 0; i < kBigRows; i++) {
        const SDL_Rect r = {kRowX, kBigTop + i * kBigPitch, kRowW, kBigH};
        AddPanelTapRow(i, r);
        const bool f = i == activeSPIdx;
        const SDL_FRect fr = ToFRect(r);
        FillRoundRect(rend, fr, 12, f ? kRowFocus : kRowIdle);
        if (f) StrokeRoundRect(rend, {fr.x + 1, fr.y + 1, fr.w - 2, fr.h - 2}, 11, 2, kIce);
        if (s.ball[i]) {
            const float d = f ? 26.f : 22.f;
            SDL_FRect b = {fr.x + 21 - d / 2, fr.y + fr.h / 2 - d / 2, d, d};
            SDL_RenderTexture(rend, s.ball[i], nullptr, &b);
        }
        DrawTextLine(rend, s.modeLabel[i], kModes[i].label, f ? kValue : kFrost, kTextShadow, r.x + 42, r.y + 14);
        DrawTextLine(rend, s.modeSub[i], kModes[i].sub, f ? kFrost : kLabel, kNoShadow, r.x + 42, r.y + 29);
    }

    DrawTextLine(rend, s.options, "OPTIONS", kLabel, kNoShadow, kRowX, kSlimTop - 14);
    SDL_SetRenderDrawColor(rend, kLine.r, kLine.g, kLine.b, kLine.a);
    SDL_FRect divider = {(float)kRowX, (float)kSlimTop - 6, (float)kRowW, 1};
    SDL_RenderFillRect(rend, &divider);

    GameSettings* gs = GameSettings::Instance();
    // What the focused setting does, where the wood panel shows it in its
    // header.
    const char* hint = nullptr;
    switch (activeSPIdx) {
    case kSPRowWorldScores: hint = "Send runs to the online board"; break;
    case kSPRowAimGuide: hint = gs->spAimGuideEnabled() ? "On: runs do not count for scores"
                                                        : "Off: runs count for highscores"; break;
    case kSPRowTheme: hint = gs->modernTheme() ? "Score panel and new cards"
                                               : "The classic game screen"; break;
    case kSPRowBubbles: hint = gs->modernBubbles() ? "Glossy bubbles, every mode"
                                                   : "The original bubbles"; break;
    case kSPRowAccount: hint = "View, copy or change your account"; break;
    }
    if (hint) DrawTextLine(rend, s.optionHint, hint, kFrost, kNoShadow, kRowX + kRowW, kSlimTop - 14, 2);

    struct Slim { int idx; const char* label; int kind; bool on; const char* value; };
    enum { kSwitch, kValueRow, kArrow };
    const Slim slims[kSlimRows] = {
        {kSPRowWorldScores, "Online highscores", kSwitch, gs->worldHighscoresEnabled(), nullptr},
        {kSPRowAimGuide, "Aim guide", kSwitch, gs->spAimGuideEnabled(), nullptr},
        {kSPRowTheme, "In-game theme", kValueRow, false, gs->modernTheme() ? "MODERN" : "ORIGINAL"},
        {kSPRowBubbles, "Bubbles", kValueRow, false, gs->modernBubbles() ? "CANDY" : "CLASSIC"},
        {kSPRowAccount, "Account code", kArrow, false, nullptr},
    };
    for (int t = 0; t < kSlimRows; t++) {
        const Slim& row = slims[t];
        const SDL_Rect r = {kRowX, kSlimTop + t * kSlimPitch, kRowW, kSlimH};
        AddPanelTapRow(row.idx, r);
        const bool f = row.idx == activeSPIdx;
        const SDL_FRect fr = ToFRect(r);
        if (f) {
            FillRoundRect(rend, fr, 8, kRowFocus);
            StrokeRoundRect(rend, {fr.x + 0.75f, fr.y + 0.75f, fr.w - 1.5f, fr.h - 1.5f}, 7.5f, 1.5f, kIce);
        }
        const int cy = r.y + r.h / 2;
        DrawTextLine(rend, s.slimLabel[t], row.label, f ? kValue : kFrost, kNoShadow, r.x + 12, cy);
        const int right = r.x + r.w - 14;
        if (row.kind == kSwitch) {
            DrawSwitch(rend, r.x + r.w - 42, cy, row.on);
        } else if (row.kind == kValueRow) {
            // The ‹ › pair, on the focused row only, says LEFT/RIGHT change it.
            const int w = DrawTextLine(rend, s.slimValue[t], row.value, f ? kValue : kFrost, kTextShadow,
                               f ? right - 10 : right, cy, 2);
            if (f) {
                DrawTextLine(rend, s.chevL, "\xE2\x80\xB9", kIce, kNoShadow, right - 10 - w - 10, cy - 1, 1);
                DrawTextLine(rend, s.chevR, "\xE2\x80\xBA", kIce, kNoShadow, right, cy - 1, 1);
            }
        } else {
            DrawTextLine(rend, s.slimValue[t], "\xE2\x80\xBA", kIce, kNoShadow, right, cy - 1, 2);
        }
    }

    menulist::DrawFooterHint(rend, s.footer,
        "UP/DOWN move    ENTER select    LEFT/RIGHT change    ESC back");
}
