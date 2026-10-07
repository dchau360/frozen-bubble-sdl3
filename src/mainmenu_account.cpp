#include "mainmenu.h"
#include "menulist.h"
#include "sdl3_compat.h"
#include "audiomixer.h"
#include "frozenbubble.h"
#include "platform.h"
#include "playeraccount.h"
#include "worldscores.h"
#include "textinput.h"
#include <algorithm>

// The "Account code" screen: this device's anonymous player account
// (src/playeraccount.h). Weekly rankings belong to the account, and the
// account is nothing but its 16-character code, so this is where a player
// reads the code down, types one in from another device, or starts over.
// Opened from the Account section of the LAN/NET server lists
// (ServerListAccountIndex()), the 1-player menu (kSPRowAccount) and the High
// Scores screen's world tabs (OpenAccountFromHighscores()), full-screen like
// the weekly rankings view.
//
// Input parity (CLAUDE.md): every button is a registered tap row, LEFT/RIGHT/
// TAB move a visibly-highlighted focus, ENTER activates, ESC backs out one
// level, and the footer spells that out.

namespace {
enum ViewButton { kViewCopy = 0, kViewUseCode, kViewNew, kViewDelete, kViewBack, kViewCount };
enum EnterButton { kEnterOk = 0, kEnterCancel, kEnterCount };
enum ConfirmButton { kConfirmNew = 0, kConfirmCancel, kConfirmCount };
enum DeleteButton { kDeleteYes = 0, kDeleteCancel, kDeleteCount };
// accountMode: 0 view, 1 typing a code, 2 confirm New account, 3 confirm
// Delete account, 4 waiting for the server to delete it.
enum { kModeView = 0, kModeEnter, kModeConfirmNew, kModeConfirmDelete, kModeDeleting };

int ButtonCount(int mode) {
    switch (mode) {
    case kModeView: return kViewCount;
    case kModeEnter: return kEnterCount;
    case kModeConfirmNew: return kConfirmCount;
    case kModeConfirmDelete: return kDeleteCount;
    default: return 0;  // deleting: nothing to press until the server answers
    }
}
}  // namespace

int MainMenu::ServerListSetNameIndex(bool isLAN) const {
    const int servers = (int)(isLAN ? discoveredServers.size() : publicServers.size());
    const int discord = (!isLAN && ServerListDiscordIndex() >= 0) ? 1 : 0;
    return 1 + servers + discord;
}

void MainMenu::OpenAccountPanel() {
    showingAccount = true;
    accountMode = 0;
    accountSelection = kViewCopy;
    accountMessage.clear();
    SDL_StopTextInput(SDL_GetKeyboardFocus());
}

void MainMenu::OpenAccountFromHighscores() {
    OpenAccountPanel();
    accountFromHighscores = true;
}

void MainMenu::CloseAccountPanel() {
    showingAccount = false;
    if (accountFromHighscores) {
        accountFromHighscores = false;
        // Straight back, without ShowScoreScreen(): that resets the screen to
        // MY SCORES, and the player came from a world tab.
        FrozenBubble::Instance()->currentState = Highscores;
    }
}

void MainMenu::AccountPanelRender() {
    auto* rend = const_cast<SDL_Renderer*>(renderer);
    menulist::SetDrawColor(rend, menulist::kDialogFill, 255);
    SDL_RenderClear(rend);
    auto text = [&](const std::string& label, int x, int y, SDL_Color color = menulist::kText,
                    int size = 13) {
        panelText.UpdateStyle(size, TTF_STYLE_NORMAL);
        panelText.UpdateColor(color, menulist::kTextShadow);
        panelText.UpdateText(rend, label.c_str(), 0);
        panelText.UpdatePosition({x, y});
        SDL_FRect r = ToFRect(*panelText.Coords());
        SDL_RenderTexture(rend, panelText.Texture(), nullptr, &r);
    };

    text("YOUR ACCOUNT", 18, 14, menulist::kGold);

    int y = 48;
    if (accountMode == 1) {
        text("Enter an account code from another device", 18, y);
        text("(16 letters and digits; dashes and spaces are fine).", 18, y + 20, menulist::kMuted);
        char line[48];
        snprintf(line, sizeof(line), "[ %s_ ]", accountCodeInput);
        text(line, 160, 180, menulist::kGold, 20);
    } else if (accountMode == kModeConfirmNew) {
        text("Start a new account on this device?", 18, y, menulist::kText, 15);
        text("Your online highscores and weekly ranking stay with the current code:", 18, y + 30, menulist::kMuted);
        text(playeraccount::FormatCode(playeraccount::Code()), 18, y + 52, menulist::kGold, 18);
        text("Write it down first if you want to come back to it.", 18, y + 84, menulist::kMuted);
        text("To remove them from the server instead, use Delete account.", 18, y + 104, menulist::kMuted);
    } else if (accountMode == kModeConfirmDelete) {
        text("Delete this account?", 18, y, menulist::kText, 15);
        text(std::string("This removes everything ") + kWorldScoresHost + " keeps for it:", 18, y + 30);
        text("your online highscores and your weekly ranking.", 18, y + 50);
        text("This device then starts a new account with a new code.", 18, y + 80, menulist::kMuted);
        text("Other servers only keep a weekly line, which ends at their next", 18, y + 100, menulist::kMuted);
        text("Monday reset. This can't be undone.", 18, y + 120, menulist::kMuted);
    } else if (accountMode == kModeDeleting) {
        text(std::string("Deleting your account from ") + kWorldScoresHost + "...", 18, y, menulist::kText, 15);
        switch (worldscores::DeleteAccountStatus()) {
        case worldscores::DeleteStatus::Done:
            accountMode = kModeView;
            accountSelection = kViewBack;
            accountMessage = "Account deleted. This device has a new code now.";
            accountMessageBad = false;
            worldscores::ClearDeleteAccountStatus();
            break;
        case worldscores::DeleteStatus::Failed:
            accountMode = kModeView;
            accountSelection = kViewBack;
            accountMessage = worldscores::DeleteAccountError() + " Nothing was changed.";
            accountMessageBad = true;
            worldscores::ClearDeleteAccountStatus();
            break;
        default:
            break;
        }
    } else {
        text("Account code", 18, y, menulist::kMuted);
        const std::string code = playeraccount::Code();
        text(code.empty() ? "(unavailable on this device)" : playeraccount::FormatCode(code),
             18, y + 20, menulist::kGold, 24);
        y += 70;
        text("Your weekly ranking and online highscores belong to this code,", 18, y);
        text("not to your name. Write it down to keep them after reinstalling,", 18, y + 22, menulist::kMuted);
        text("or enter it on another device with \"Use another code\".", 18, y + 42, menulist::kMuted);
        text("Anyone who has the code can play as you. The game never", 18, y + 72, menulist::kMuted);
        text("sends it anywhere: servers only see a key made from it.", 18, y + 92, menulist::kMuted);
    }
    if (!accountMessage.empty())
        text(accountMessage, 18, 340, accountMessageBad ? menulist::kBad : menulist::kGold);

    static const char* kView[kViewCount] = {"Copy code", "Use another code", "New account",
                                            "Delete account", "Back"};
    static const char* kEnter[kEnterCount] = {"Use this code", "Cancel"};
    static const char* kConfirm[kConfirmCount] = {"New account", "Cancel"};
    static const char* kDelete[kDeleteCount] = {"Delete account", "Cancel"};
    const char* const* labels = accountMode == kModeView ? kView
                              : accountMode == kModeEnter ? kEnter
                              : accountMode == kModeConfirmNew ? kConfirm : kDelete;
    const int count = ButtonCount(accountMode);
    if (count > 0) {
        accountSelection = std::clamp(accountSelection, 0, count - 1);
        BeginPanelTapRows(&accountSelection);
        // The row's 604px shared out evenly, so five fit as well as two.
        constexpr int kGap = 7;
        const int w = (604 - (count - 1) * kGap) / count;
        for (int i = 0; i < count; ++i) {
            SDL_Rect r = {18 + i * (w + kGap), 396, w, 26};
            const bool sel = i == accountSelection;
            menulist::SetDrawColor(rend, sel ? menulist::kButtonFocus : menulist::kButtonFill, 255);
            auto fr = ToFRect(r);
            SDL_RenderFillRect(rend, &fr);
            panelText.UpdateStyle(13, TTF_STYLE_NORMAL);
            panelText.UpdateText(rend, labels[i], 0);
            text(labels[i], r.x + (r.w - panelText.Coords()->w) / 2, r.y + 3,
                 sel ? menulist::kGold : menulist::kText);
            AddPanelTapRow(i, r);
        }
    }
    menulist::DrawFooterHint(rend, panelText, accountMode == kModeEnter
        ? "TYPE THE CODE   ENTER: USE IT   ESC / B: CANCEL"
        : accountMode == kModeDeleting ? "PLEASE WAIT"
        : "LEFT/RIGHT: MOVE   ENTER / A: SELECT   ESC / B: BACK");
    panelText.UpdateStyle(15, TTF_STYLE_NORMAL);
}

bool MainMenu::AccountPanelKey(SDL_Event* e) {
    if (!showingAccount) return false;
    if (e->type == SDL_EVENT_TEXT_INPUT) {
        if (accountMode == 1) AppendUtf8Input(accountCodeInput, e->text.text, 22);
        return true;
    }
    if (e->type != SDL_EVENT_KEY_DOWN) return false;
    // Waiting on the server: the result decides what the screen says next,
    // so nothing (not even ESC) leaves it half-done.
    if (accountMode == kModeDeleting) return true;
    const auto key = e->key.key;
    const int count = ButtonCount(accountMode);

    auto useTyped = [&](const char* typed) {
        if (playeraccount::UseCode(typed)) {
            accountMessage = "Now using that account on this device.";
            accountMessageBad = false;
            accountMode = 0;
            accountSelection = kViewBack;
            SDL_StopTextInput(SDL_GetKeyboardFocus());
            AudioMixer::Instance()->PlaySFX("menu_selected");
        } else {
            accountMessage = "That isn't a valid code: it needs 16 letters and digits.";
            accountMessageBad = true;
            AudioMixer::Instance()->PlaySFX("cancel");
        }
    };

    if (accountMode == 1 && (key == SDLK_BACKSPACE || key == SDLK_DELETE)) {
        BackspaceUtf8(accountCodeInput);
        return true;
    }
    if (key == SDLK_ESCAPE || key == SDLK_AC_BACK) {
        AudioMixer::Instance()->PlaySFX("cancel");
        if (accountMode == 0) {
            CloseAccountPanel();
        } else {
            accountMode = 0;
            accountSelection = kViewBack;
            SDL_StopTextInput(SDL_GetKeyboardFocus());
        }
        return true;
    }
    if (key == SDLK_LEFT || key == SDLK_UP) {
        accountSelection = (accountSelection + count - 1) % count;
        AudioMixer::Instance()->PlaySFX("menu_change");
        return true;
    }
    if (key == SDLK_RIGHT || key == SDLK_DOWN || key == SDLK_TAB) {
        accountSelection = (accountSelection + 1) % count;
        AudioMixer::Instance()->PlaySFX("menu_change");
        return true;
    }
    if (key != SDLK_RETURN && key != SDLK_KP_ENTER && !(key == SDLK_SPACE && accountMode != 1))
        return true;  // modal: nothing else leaks through to the list underneath

    if (accountMode == 1) {
        if (accountSelection == kEnterCancel) {
            accountMode = 0;
            accountSelection = kViewUseCode;
            SDL_StopTextInput(SDL_GetKeyboardFocus());
            AudioMixer::Instance()->PlaySFX("cancel");
        } else {
            useTyped(accountCodeInput);
        }
        return true;
    }
    if (accountMode == 2) {
        accountMode = 0;
        if (accountSelection == kConfirmNew) {
            if (playeraccount::StartNewAccount()) {
                accountMessage = "New account started. Its code is shown above.";
                accountMessageBad = false;
            } else {
                accountMessage = "Couldn't create a new account on this device.";
                accountMessageBad = true;
            }
            AudioMixer::Instance()->PlaySFX("menu_selected");
        } else {
            AudioMixer::Instance()->PlaySFX("cancel");
        }
        accountSelection = kViewBack;
        return true;
    }
    if (accountMode == kModeConfirmDelete) {
        if (accountSelection == kDeleteYes) {
            AudioMixer::Instance()->PlaySFX("menu_selected");
            accountMessage.clear();
            accountMode = kModeDeleting;
            worldscores::RequestDeleteAccount();
        } else {
            AudioMixer::Instance()->PlaySFX("cancel");
            accountMode = kModeView;
            accountSelection = kViewDelete;
        }
        return true;
    }

    AudioMixer::Instance()->PlaySFX("menu_selected");
    switch (accountSelection) {
    case kViewCopy:
#ifdef __WASM_PORT__
        // SDL's clipboard call reports success in the browser even when the
        // page is in a frame that may not write the clipboard (itch.io), so
        // the copy never happened. WasmCopyText knows; when it fails, the
        // code is shown in a browser box the player can copy from instead.
        {
            const std::string formatted = playeraccount::FormatCode(playeraccount::Code());
            if (WasmCopyText(formatted.c_str())) {
                accountMessage = "Copied to the clipboard.";
                accountMessageBad = false;
            } else {
                char ignored[64];
                WasmPromptText("Copy your account code (select it, then copy):",
                               formatted.c_str(), ignored, sizeof(ignored));
                accountMessage = "Copy it from the box, or write it down.";
                accountMessageBad = false;
            }
        }
        break;
#endif
        if (SDL_SetClipboardText(playeraccount::FormatCode(playeraccount::Code()).c_str())) {
            accountMessage = "Copied to the clipboard.";
            accountMessageBad = false;
        } else {
            accountMessage = "Couldn't copy here; write the code down instead.";
            accountMessageBad = true;
        }
        break;
    case kViewUseCode:
        accountMessage.clear();
#ifdef __WASM_PORT__
        if (WasmHasTouch()) {
            // Native prompt on touch devices (no soft keyboard in SDL3
            // Emscripten), same as the nickname fields.
            char typed[64];
            if (WasmPromptText("Account code from your other device:", "", typed, sizeof(typed)))
                useTyped(typed);
            break;
        }
#endif
        accountCodeInput[0] = '\0';
        accountMode = 1;
        accountSelection = kEnterOk;
        SDL_StopTextInput(SDL_GetKeyboardFocus());
        SDL_StartTextInput(SDL_GetKeyboardFocus());
        SetTextInputAreaLogical(const_cast<SDL_Renderer*>(renderer), {160, 180, 320, 24});
        break;
    case kViewNew:
        accountMessage.clear();
        accountMode = kModeConfirmNew;
        accountSelection = kConfirmCancel;  // the destructive choice is never the default
        break;
    case kViewDelete:
        accountMessage.clear();
        accountMode = kModeConfirmDelete;
        accountSelection = kDeleteCancel;  // never the default either
        break;
    default:
        CloseAccountPanel();
        break;
    }
    return true;
}
