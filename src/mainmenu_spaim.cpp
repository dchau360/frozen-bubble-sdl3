#include "mainmenu.h"
#include "menulist.h"
#include "sdl3_compat.h"
#include "audiomixer.h"
#include "gamesettings.h"
#include "worldscores.h"

// "Aim guide is on" prompt: a 1-player run played with the aim guide on never
// reaches a highscore table (BubbleGame::RunCountsForScores), so START asks
// first whether to turn it off. Applies to the scored paths only -- the
// campaign START, Pick start level and Multiplayer training; Random levels
// never record a score, so it doesn't ask there.
//
// Input parity (CLAUDE.md): both buttons are registered tap rows, LEFT/RIGHT/
// TAB move a visibly-highlighted focus, ENTER / A activates it, ESC / B backs
// out to the menu, and the footer spells that out.

namespace {
enum { kTurnOff = 0, kPlayAnyway, kButtons };
}  // namespace

void MainMenu::ApplySoloAim(SetupSettings& settings) const {
    settings.aimGuide[0] = GameSettings::Instance()->spAimGuideEnabled();
}

void MainMenu::BeginSoloStart(int mode) {
    if (GameSettings::Instance()->spAimGuideEnabled()) {
        spAimPrompt = true;
        spAimFocus = kTurnOff;
        spAimPendingMode = mode;
        return;
    }
    ContinueSoloStart(mode);
}

void MainMenu::ContinueSoloStart(int mode) {
    switch (mode) {
        case 5:  // Pick start level: open number input panel
            showingLevelPanel = true;
            levelInput.clear();
            runDelay = false;
            SDL_StartTextInput(SDL_GetKeyboardFocus());
            break;
        case 6:  // mp_training: ask chain reaction then start
            showingSPPanel = false;
            showingOptPanel = awaitKp = true;
            panelText.UpdateText(const_cast<SDL_Renderer*>(renderer),
                "Multiplayer training\n\n\nEnable chain reaction?\n\n\nY or N?:          \n", 0);
            panelText.UpdatePosition({(640/2) - (panelText.Coords()->w / 2), (480/2) - 120});
            selectedMode = 6;  // mode 6 = mp_training
            break;
        default:
            if (SPNamePromptApplies()) { OpenSPNamePrompt(); return; }
            SetupNewGame(1);
            break;
    }
}

void MainMenu::FinishSPAimPrompt(bool turnOff) {
    spAimPrompt = false;
    if (turnOff) GameSettings::Instance()->SetValue("Game:SPAimGuide", "");
    ContinueSoloStart(spAimPendingMode);
}

void MainMenu::SPAimPromptRender() {
    auto* rend = const_cast<SDL_Renderer*>(renderer);
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(rend, 0, 0, 0, 160);
    SDL_RenderFillRect(rend, nullptr);

    const SDL_Rect box = {110, 140, 420, 180};
    SDL_SetRenderDrawColor(rend, 17, 26, 45, 245);
    SDL_FRect fbox = ToFRect(box);
    SDL_RenderFillRect(rend, &fbox);
    SDL_SetRenderDrawColor(rend, menulist::kGold.r, menulist::kGold.g, menulist::kGold.b, 255);
    SDL_RenderRect(rend, &fbox);

    auto text = [&](const char* label, int y, SDL_Color color, int size) {
        panelText.UpdateStyle(size, TTF_STYLE_NORMAL);
        panelText.UpdateColor(color, menulist::kTextShadow);
        panelText.UpdateText(rend, label, 0);
        panelText.UpdatePosition({320 - panelText.Coords()->w / 2, y});
        SDL_FRect r = ToFRect(*panelText.Coords());
        SDL_RenderTexture(rend, panelText.Texture(), nullptr, &r);
    };
    text("AIM GUIDE IS ON", box.y + 14, menulist::kGold, 15);
    text("Runs played with the aim guide do not count", box.y + 48, menulist::kText, 13);
    text("on any highscore table.", box.y + 68, menulist::kText, 13);
    text("Turn it off before you start?", box.y + 94, menulist::kMuted, 13);

    static const char* kLabels[kButtons] = {"Turn off", "Play without scores"};
    BeginPanelTapRows(&spAimFocus);
    for (int i = 0; i < kButtons; ++i) {
        const SDL_Rect r = {box.x + 20 + i * 190, box.y + 126, 180, 32};
        const bool sel = i == spAimFocus;
        SDL_SetRenderDrawColor(rend, sel ? 94 : 35, 69, 76, 255);
        SDL_FRect fr = ToFRect(r);
        SDL_RenderFillRect(rend, &fr);
        if (sel) {
            SDL_SetRenderDrawColor(rend, menulist::kGold.r, menulist::kGold.g, menulist::kGold.b, 255);
            SDL_RenderRect(rend, &fr);
        }
        panelText.UpdateStyle(14, TTF_STYLE_NORMAL);
        panelText.UpdateColor(sel ? menulist::kGold : menulist::kText, menulist::kTextShadow);
        panelText.UpdateText(rend, kLabels[i], 0);
        panelText.UpdatePosition({r.x + (r.w - panelText.Coords()->w) / 2,
                                  r.y + (r.h - panelText.Coords()->h) / 2});
        SDL_FRect tr = ToFRect(*panelText.Coords());
        SDL_RenderTexture(rend, panelText.Texture(), nullptr, &tr);
        AddPanelTapRow(i, r);
    }
    menulist::DrawFooterHint(rend, panelText,
        "LEFT/RIGHT: MOVE   ENTER / A: SELECT   ESC / B: BACK");
    panelText.UpdateStyle(15, TTF_STYLE_NORMAL);
}

bool MainMenu::SPAimPromptKey(SDL_Event* e) {
    if (!spAimPrompt) return false;
    if (e->type != SDL_EVENT_KEY_DOWN) return e->type == SDL_EVENT_TEXT_INPUT;
    if (e->key.repeat) return true;
    const SDL_Keycode key = e->key.key;
    if (key == SDLK_ESCAPE || key == SDLK_AC_BACK) {
        AudioMixer::Instance()->PlaySFX("cancel");
        spAimPrompt = false;  // back to the 1-player menu, nothing started
        return true;
    }
    if (key == SDLK_LEFT || key == SDLK_RIGHT || key == SDLK_TAB) {
        spAimFocus = spAimFocus == kTurnOff ? kPlayAnyway : kTurnOff;
        AudioMixer::Instance()->PlaySFX("menu_change");
        return true;
    }
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        AudioMixer::Instance()->PlaySFX("menu_selected");
        FinishSPAimPrompt(spAimFocus == kTurnOff);
        return true;
    }
    return true;  // modal
}
