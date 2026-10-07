#include "mainmenu.h"
#include "menulist.h"
#include "sdl3_compat.h"
#include "audiomixer.h"
#include "gamesettings.h"
#include "platform.h"
#include "worldscores.h"
#include <cstdlib>
#include <cstring>

// "Name for the world board": START in the 1-player menu opens this when a
// classic run would go on the world highscores (worldscores::SendingEnabled())
// and the player has never set a name, which used to put every such run on
// the public board as "unnamed". Asked once per session -- Skip starts the
// game and the run is sent as "unnamed" -- and never again once a name exists.
// The name is the same one online play uses (SavePreNick).
//
// Input parity (CLAUDE.md): both buttons are registered tap rows, LEFT/RIGHT/
// TAB move a visibly-highlighted focus, ENTER / A activates it, ESC / B skips,
// and the footer spells that out.

namespace {
// What fb-server's is_nick_ok() accepts, and so all the world board can show
// (worldscores::SubmitNick drops anything else): 1-10 of [A-Za-z0-9_-]. Only
// those can be typed here, so what is on screen is what the board shows.
constexpr size_t kMaxName = 10;
bool NameChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-';
}
void AppendName(char* buf, const char* text) {
    size_t len = strlen(buf);
    for (const char* p = text; *p && len < kMaxName; ++p)
        if (NameChar(*p)) buf[len++] = *p;
    buf[len] = '\0';
}
enum { kSave = 0, kSkip, kButtons };
}  // namespace

bool MainMenu::SPNamePromptApplies() const {
    return !spNamePromptAsked && worldscores::SendingEnabled() &&
           GameSettings::Instance()->savedNickname[0] == '\0';
}

void MainMenu::OpenSPNamePrompt() {
    spNamePromptAsked = true;
    spNameInput[0] = '\0';
    // Prefilled the way the online name editor is, so on a desktop it is
    // usually one ENTER.
    if (const char* user = getenv("USER")) AppendName(spNameInput, user);
#ifdef __WASM_PORT__
    if (WasmHasTouch()) {
        // No soft keyboard in SDL3's Emscripten backend: a native prompt,
        // like the other name fields. Cancel or an empty answer is a skip.
        char typed[32];
        spNameInput[0] = '\0';
        if (WasmPromptText("Name for the online highscores (up to 10 letters/digits):",
                           "", typed, sizeof(typed)))
            AppendName(spNameInput, typed);
        FinishSPNamePrompt(spNameInput[0] != '\0');
        return;
    }
#endif
    spNamePrompt = true;
    spNameFocus = kSave;
    SDL_StopTextInput(SDL_GetKeyboardFocus());
    SDL_StartTextInput(SDL_GetKeyboardFocus());
    SetTextInputAreaLogical(const_cast<SDL_Renderer*>(renderer), {200, 222, 240, 26});
}

void MainMenu::FinishSPNamePrompt(bool save) {
    spNamePrompt = false;
    SDL_StopTextInput(SDL_GetKeyboardFocus());
    if (save && spNameInput[0] != '\0') {
        snprintf(networkPreNick, sizeof(networkPreNick), "%s", spNameInput);
        SavePreNick();  // the shared name, and GameSettings::savedNickname
    }
    SetupNewGame(1);
}

void MainMenu::SPNamePromptRender() {
    auto* rend = const_cast<SDL_Renderer*>(renderer);
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(rend, 0, 0, 0, 160);
    SDL_RenderFillRect(rend, nullptr);

    const SDL_Rect box = {110, 120, 420, 220};
    menulist::SetDrawColor(rend, menulist::kDialogFill, 245);
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
    text("NAME FOR THE ONLINE BOARD", box.y + 14, menulist::kGold, 15);
    text("Your classic runs go on this port's online highscores.", box.y + 44, menulist::kText, 13);
    text("Up to 10 letters, digits, _ or -. Skip to play as \"unnamed\".", box.y + 64,
         menulist::kMuted, 13);
    char line[32];
    snprintf(line, sizeof(line), "[ %s_ ]", spNameInput);
    text(line, box.y + 100, menulist::kGold, 20);

    static const char* kLabels[kButtons] = {"Save and play", "Skip"};
    BeginPanelTapRows(&spNameFocus);
    for (int i = 0; i < kButtons; ++i) {
        const SDL_Rect r = {box.x + 40 + i * 180, box.y + 162, 160, 32};
        const bool sel = i == spNameFocus;
        // A Save with nothing typed would do nothing, so it is drawn dimmed.
        const bool dead = i == kSave && spNameInput[0] == '\0';
        menulist::SetDrawColor(rend, sel ? menulist::kButtonFocus : menulist::kButtonFill, 255);
        SDL_FRect fr = ToFRect(r);
        SDL_RenderFillRect(rend, &fr);
        if (sel) {
            SDL_SetRenderDrawColor(rend, menulist::kGold.r, menulist::kGold.g, menulist::kGold.b, 255);
            SDL_RenderRect(rend, &fr);
        }
        panelText.UpdateStyle(14, TTF_STYLE_NORMAL);
        panelText.UpdateColor(dead ? menulist::kMuted : sel ? menulist::kGold : menulist::kText,
                              menulist::kTextShadow);
        panelText.UpdateText(rend, kLabels[i], 0);
        panelText.UpdatePosition({r.x + (r.w - panelText.Coords()->w) / 2,
                                  r.y + (r.h - panelText.Coords()->h) / 2});
        SDL_FRect tr = ToFRect(*panelText.Coords());
        SDL_RenderTexture(rend, panelText.Texture(), nullptr, &tr);
        AddPanelTapRow(i, r);
    }
    menulist::DrawFooterHint(rend, panelText,
        "TYPE A NAME   LEFT/RIGHT: MOVE   ENTER / A: SELECT   ESC / B: SKIP");
    panelText.UpdateStyle(15, TTF_STYLE_NORMAL);
}

bool MainMenu::SPNamePromptKey(SDL_Event* e) {
    if (!spNamePrompt) return false;
    if (e->type == SDL_EVENT_TEXT_INPUT) {
        AppendName(spNameInput, e->text.text);
        return true;
    }
    if (e->type != SDL_EVENT_KEY_DOWN) return false;
    const SDL_Keycode key = e->key.key;
    if (key == SDLK_BACKSPACE || key == SDLK_DELETE) {
        const size_t len = strlen(spNameInput);
        if (len > 0) spNameInput[len - 1] = '\0';
        return true;
    }
    if (e->key.repeat) return true;
    if (key == SDLK_ESCAPE || key == SDLK_AC_BACK) {
        AudioMixer::Instance()->PlaySFX("cancel");
        FinishSPNamePrompt(false);
        return true;
    }
    if (key == SDLK_LEFT || key == SDLK_RIGHT || key == SDLK_TAB) {
        spNameFocus = spNameFocus == kSave ? kSkip : kSave;
        AudioMixer::Instance()->PlaySFX("menu_change");
        return true;
    }
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        if (spNameFocus == kSave && spNameInput[0] == '\0') {
            AudioMixer::Instance()->PlaySFX("cancel");  // nothing to save yet
            return true;
        }
        AudioMixer::Instance()->PlaySFX("menu_selected");
        FinishSPNamePrompt(spNameFocus == kSave);
        return true;
    }
    return true;  // modal: nothing leaks through to the 1-player menu underneath
}
