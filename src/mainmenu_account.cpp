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
#include <cctype>
#include <cstring>

// The "Account code" screen: this device's anonymous player account
// (src/playeraccount.h). Weekly rankings belong to the account, and the
// account is nothing but its 16-character code, so this is where a player
// reads the code down, types one in from another device, or starts over.
// Opened from the Account section of the LAN/NET server lists
// (ServerListAccountIndex()), the 1-player menu (kSPRowAccount) and the High
// Scores screen's world tabs (OpenAccountFromHighscores()), full-screen like
// the weekly rankings view.
//
// Set PIN / Link with PIN (worldscores::RequestSetPin/RequestLinkPin,
// server/links.h) are the short way to the same account on another device:
// a name and a PIN instead of the code.
//
// Input parity (CLAUDE.md): every button is a registered tap row, LEFT/RIGHT/
// TAB move a visibly-highlighted focus (UP/DOWN between the view's two rows),
// ENTER activates, ESC backs out one level, and the footer spells that out.

namespace {
// The view's buttons, in two rows: the first four on top, the rest below.
enum ViewButton { kViewCopy = 0, kViewUseCode, kViewSetPin, kViewLinkPin,
                  kViewNew, kViewDelete, kViewBack, kViewCount };
constexpr int kViewTopRow = kViewNew;
enum EnterButton { kEnterOk = 0, kEnterCancel, kEnterCount };
enum ConfirmButton { kConfirmNew = 0, kConfirmCancel, kConfirmCount };
enum DeleteButton { kDeleteYes = 0, kDeleteCancel, kDeleteCount };
// accountMode: 0 view, 1 typing a code, 2 confirm New account, 3 confirm
// Delete account, 4 waiting for the server to delete it, 5 typing a new PIN,
// 6/7 typing the name / PIN to link with, 8 waiting for the server's answer.
enum { kModeView = 0, kModeEnter, kModeConfirmNew, kModeConfirmDelete, kModeDeleting,
       kModeSetPin, kModeLinkName, kModeLinkPin, kModePinWorking };

bool IsTyping(int mode) {
    return mode == kModeEnter || mode == kModeSetPin || mode == kModeLinkName || mode == kModeLinkPin;
}

int ButtonCount(int mode) {
    switch (mode) {
    case kModeView: return kViewCount;
    case kModeConfirmNew: return kConfirmCount;
    case kModeConfirmDelete: return kDeleteCount;
    default: return IsTyping(mode) ? kEnterCount : 0;  // waiting: nothing to press
    }
}

// What fb-server's is_nick_ok() takes, the shape of a name a PIN is set under.
bool NameShapeOk(const char* name) {
    const size_t n = std::strlen(name);
    if (n < 1 || n > 10) return false;
    for (const char* c = name; *c; ++c)
        if (!std::isalnum(static_cast<unsigned char>(*c)) && *c != '_' && *c != '-') return false;
    return true;
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

    text("YOUR ACCOUNT", menulist::kBackBtn.x + menulist::kBackBtn.w + 12, 14, menulist::kGold);

    int y = 48;
    if (accountMode == kModeSetPin || accountMode == kModeLinkPin) {
        if (accountMode == kModeSetPin) {
            text("Choose a PIN of 4 to 8 digits for " + worldscores::SubmitNick() + ".", 18, y);
            text("On another device, Link with PIN and enter this name and PIN", 18, y + 20, menulist::kMuted);
            text("to play as this account there, no code needed.", 18, y + 40, menulist::kMuted);
            text("Keep it simple but not obvious: it is only as safe as a PIN.", 18, y + 70, menulist::kMuted);
        } else {
            text(std::string("The PIN set for ") + accountLinkName + ":", 18, y);
        }
        std::string masked(std::strlen(accountCodeInput), '*');
        text("[ " + masked + "_ ]", 240, 180, menulist::kGold, 20);
    } else if (accountMode == kModeLinkName) {
        text("Play as an account that has a PIN.", 18, y);
        text("Enter the name its PIN was set under on the other device:", 18, y + 20, menulist::kMuted);
        text("This device's own scores are added to that account.", 18, y + 50, menulist::kMuted);
        char line[48];
        snprintf(line, sizeof(line), "[ %s_ ]", accountCodeInput);
        text(line, 200, 180, menulist::kGold, 20);
    } else if (accountMode == kModePinWorking) {
        text(std::string("Asking ") + kWorldScoresHost + "...", 18, y, menulist::kText, 15);
        switch (worldscores::PinRequestStatus()) {
        case worldscores::PinStatus::Done:
        case worldscores::PinStatus::Failed:
            accountMessageBad = worldscores::PinRequestStatus() == worldscores::PinStatus::Failed;
            accountMessage = worldscores::PinRequestMessage();
            accountMode = kModeView;
            accountSelection = kViewBack;
            worldscores::ClearPinRequestStatus();
            break;
        default:
            break;
        }
    } else if (accountMode == 1) {
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
        text("No code at hand? Set PIN here, then Link with PIN on the other device.", 18, y + 122);
    }
    if (!accountMessage.empty())
        text(accountMessage, 18, 334, accountMessageBad ? menulist::kBad : menulist::kGold);

    static const char* kView[kViewCount] = {"Copy code", "Use another code", "Set PIN",
                                            "Link with PIN", "New account", "Delete account", "Back"};
    static const char* kEnter[kEnterCount] = {"Use this code", "Cancel"};
    static const char* kSetPin[kEnterCount] = {"Set PIN", "Cancel"};
    static const char* kLinkName[kEnterCount] = {"Next", "Cancel"};
    static const char* kLinkPin[kEnterCount] = {"Link", "Cancel"};
    static const char* kConfirm[kConfirmCount] = {"New account", "Cancel"};
    static const char* kDelete[kDeleteCount] = {"Delete account", "Cancel"};
    const char* const* labels = accountMode == kModeView ? kView
                              : accountMode == kModeEnter ? kEnter
                              : accountMode == kModeSetPin ? kSetPin
                              : accountMode == kModeLinkName ? kLinkName
                              : accountMode == kModeLinkPin ? kLinkPin
                              : accountMode == kModeConfirmNew ? kConfirm : kDelete;
    const int count = ButtonCount(accountMode);
    if (count > 0) {
        accountSelection = std::clamp(accountSelection, 0, count - 1);
        BeginPanelTapRows(&accountSelection);
        // Each row's 604px shared out evenly; the view's seven go in two rows.
        constexpr int kGap = 7;
        for (int i = 0; i < count; ++i) {
            const bool twoRows = accountMode == kModeView;
            const bool top = twoRows && i < kViewTopRow;
            const int inRow = !twoRows ? count : top ? kViewTopRow : count - kViewTopRow;
            const int col = top || !twoRows ? i : i - kViewTopRow;
            const int w = (604 - (inRow - 1) * kGap) / inRow;
            SDL_Rect r = {18 + col * (w + kGap), top ? 362 : 396, w, 26};
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
        : accountMode == kModeLinkName ? "TYPE THE NAME   ENTER: NEXT   ESC / B: CANCEL"
        : IsTyping(accountMode) ? "TYPE THE PIN   ENTER: OK   ESC / B: CANCEL"
        : accountMode == kModeDeleting || accountMode == kModePinWorking ? "PLEASE WAIT"
        : accountMode == kModeView ? "ARROWS: MOVE   ENTER / A: SELECT   ESC / B: BACK"
        : "LEFT/RIGHT: MOVE   ENTER / A: SELECT   ESC / B: BACK");
    panelText.UpdateStyle(15, TTF_STYLE_NORMAL);
}

bool MainMenu::AccountPanelKey(SDL_Event* e) {
    if (!showingAccount) return false;
    if (e->type == SDL_EVENT_TEXT_INPUT) {
        if (accountMode == 1) {
            AppendUtf8Input(accountCodeInput, e->text.text, 22);
        } else if (IsTyping(accountMode)) {
            // Only what the server takes: digits for a PIN, is_nick_ok()'s
            // characters for a name.
            const bool pin = accountMode != kModeLinkName;
            const size_t cap = pin ? 8 : 10;
            for (const char* c = e->text.text; *c; ++c) {
                const bool ok = pin ? std::isdigit(static_cast<unsigned char>(*c))
                                    : std::isalnum(static_cast<unsigned char>(*c)) || *c == '_' || *c == '-';
                const size_t n = std::strlen(accountCodeInput);
                if (ok && n < cap) { accountCodeInput[n] = *c; accountCodeInput[n + 1] = '\0'; }
            }
        }
        return true;
    }
    if (e->type != SDL_EVENT_KEY_DOWN) return false;
    // Waiting on the server: the result decides what the screen says next,
    // so nothing (not even ESC) leaves it half-done.
    if (accountMode == kModeDeleting || accountMode == kModePinWorking) return true;
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

    // Opens the editor for one of the typed modes, empty.
    auto startTyping = [&](int mode, SDL_Rect area) {
        accountCodeInput[0] = '\0';
        accountMode = mode;
        accountSelection = kEnterOk;
        SDL_StopTextInput(SDL_GetKeyboardFocus());
        SDL_StartTextInput(SDL_GetKeyboardFocus());
        SetTextInputAreaLogical(const_cast<SDL_Renderer*>(renderer), area);
    };
    auto setPin = [&](const char* pin) {
        if (!worldscores::PinShapeOk(pin)) {
            accountMessage = "A PIN is 4 to 8 digits.";
            accountMessageBad = true;
            AudioMixer::Instance()->PlaySFX("cancel");
            return;
        }
        SDL_StopTextInput(SDL_GetKeyboardFocus());
        accountMessage.clear();
        accountMode = kModePinWorking;
        worldscores::RequestSetPin(pin);
        AudioMixer::Instance()->PlaySFX("menu_selected");
    };
    auto linkName = [&](const char* name) -> bool {
        if (!NameShapeOk(name)) {
            accountMessage = "A name is 1 to 10 letters, digits, _ or -.";
            accountMessageBad = true;
            AudioMixer::Instance()->PlaySFX("cancel");
            return false;
        }
        snprintf(accountLinkName, sizeof(accountLinkName), "%s", name);
        accountMessage.clear();
        return true;
    };
    auto linkPin = [&](const char* pin) {
        if (!worldscores::PinShapeOk(pin)) {
            accountMessage = "A PIN is 4 to 8 digits.";
            accountMessageBad = true;
            AudioMixer::Instance()->PlaySFX("cancel");
            return;
        }
        SDL_StopTextInput(SDL_GetKeyboardFocus());
        accountMessage.clear();
        accountMode = kModePinWorking;
        worldscores::RequestLinkPin(accountLinkName, pin);
        AudioMixer::Instance()->PlaySFX("menu_selected");
    };

    if (IsTyping(accountMode) && (key == SDLK_BACKSPACE || key == SDLK_DELETE)) {
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
    if (accountMode == kModeView && (key == SDLK_UP || key == SDLK_DOWN)) {
        // Between the two rows, to the button nearest the same column.
        accountSelection = accountSelection < kViewTopRow
            ? kViewTopRow + std::min(accountSelection, kViewCount - kViewTopRow - 1)
            : std::min(accountSelection - kViewTopRow, kViewTopRow - 1);
        AudioMixer::Instance()->PlaySFX("menu_change");
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
    if (key != SDLK_RETURN && key != SDLK_KP_ENTER && !(key == SDLK_SPACE && !IsTyping(accountMode)))
        return true;  // modal: nothing else leaks through to the list underneath

    if (accountMode == kModeSetPin || accountMode == kModeLinkName || accountMode == kModeLinkPin) {
        if (accountSelection == kEnterCancel) {
            const int back = accountMode == kModeSetPin ? kViewSetPin : kViewLinkPin;
            accountMode = kModeView;
            accountSelection = back;
            accountCodeInput[0] = '\0';
            SDL_StopTextInput(SDL_GetKeyboardFocus());
            AudioMixer::Instance()->PlaySFX("cancel");
        } else if (accountMode == kModeSetPin) {
            setPin(accountCodeInput);
        } else if (accountMode == kModeLinkName) {
            if (linkName(accountCodeInput)) {
                AudioMixer::Instance()->PlaySFX("menu_selected");
                startTyping(kModeLinkPin, {240, 180, 160, 24});
            }
        } else {
            linkPin(accountCodeInput);
        }
        if (accountMode == kModePinWorking) accountCodeInput[0] = '\0';
        return true;
    }

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
        startTyping(kModeEnter, {160, 180, 320, 24});
        break;
    case kViewSetPin:
        accountMessage.clear();
        if (worldscores::SubmitNick() == "unnamed") {
            // The PIN is found by name, so it needs one the player chose.
            accountMessage = "Set a name first (Set name in the NET GAME list), then a PIN.";
            accountMessageBad = true;
            break;
        }
#ifdef __WASM_PORT__
        if (WasmHasTouch()) {
            char typed[16];
            if (WasmPromptText("New PIN (4 to 8 digits):", "", typed, sizeof(typed)))
                setPin(typed);
            break;
        }
#endif
        startTyping(kModeSetPin, {240, 180, 160, 24});
        break;
    case kViewLinkPin:
        accountMessage.clear();
#ifdef __WASM_PORT__
        if (WasmHasTouch()) {
            char typed[16];
            if (WasmPromptText("Name the PIN was set under:", "", typed, sizeof(typed)) && linkName(typed)
                && WasmPromptText("PIN:", "", typed, sizeof(typed)))
                linkPin(typed);
            break;
        }
#endif
        startTyping(kModeLinkName, {200, 180, 240, 24});
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
