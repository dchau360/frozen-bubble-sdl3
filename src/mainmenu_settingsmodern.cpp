#include "mainmenu.h"
#include "mainmenu_internal.h"
#include "menulist.h"
#include "modernui.h"
#include "sdl3_compat.h"
#include "gamesettings.h"
#include "platform.h"
#include <algorithm>
#include <cstdio>
#include <deque>
#include <string>

// CONTROLS & SETTINGS in the Modern menu style (MENU_THEME_MODERN): a top bar,
// a Controls card (the four players as tabs, each binding drawn as a key cap,
// Reset to defaults, the connected-controller count) and a Game card (speed,
// switches, the Android store, Replays, Reset all settings). Drawing only:
// every row keeps its KeyConfigRow index and the player tabs their
// kKeyPlayerTapBase rows, so KeysPanelKey and HandlePanelTap are the classic
// panel's.
//
// Input parity (CLAUDE.md): UP/DOWN walk the rows, ENTER changes, LEFT/RIGHT
// step the speed or switch player, 1-4 jump to a player, ESC closes; every row
// and tab is a tap row; the focused row has the ice edge and the focused tab
// the white ring; the footer spells it out.

namespace {
using namespace modernui;

constexpr SDL_Rect kTopBar = {12, 10, 616, 42};
// Both cards start under the top bar and end together, as low as the taller
// one's content needs and never past the footer.
constexpr int kCardTop = 60, kCardBottomMax = 446;
constexpr int kControlsX = 12, kControlsW = 300, kGameX = 320, kGameW = 308;
constexpr int kKeyPitch = 46, kKeyH = 38;
}  // namespace

struct MainMenu::SettingsModernText {
    TTF_Font *title = nullptr, *value = nullptr, *small = nullptr;
    TTF_Font *body = nullptr, *chat = nullptr, *tiny = nullptr;
    std::deque<TTFText> pool;
    size_t used = 0;

    TTFText& Take(TTF_Font* f) {
        if (used == pool.size()) pool.emplace_back();
        TTFText& t = pool[used++];
        t.LoadFont(f);
        return t;
    }
};

void MainMenu::FreeSettingsModern() {
    if (!settingsModern) return;
    SettingsModernText* s = settingsModern;
    settingsModern = nullptr;
    TTF_Font* fonts[] = {s->title, s->value, s->small, s->body, s->chat, s->tiny};
    delete s;  // the TTFTexts only borrow the fonts, so they go first
    for (TTF_Font* f : fonts) if (f) TTF_CloseFont(f);
}

void MainMenu::KeysPanelRenderModern() {
    if (!settingsModern) {
        settingsModern = new SettingsModernText;
        SettingsModernText& s = *settingsModern;
        const std::string display = ASSET("/gfx/Baloo2-ExtraBold.ttf");
        const std::string body = ASSET("/gfx/DroidSans.ttf");
        s.title = TTF_OpenFont(display.c_str(), 20);
        s.value = TTF_OpenFont(display.c_str(), 14);
        s.small = TTF_OpenFont(display.c_str(), 12);
        s.body = TTF_OpenFont(body.c_str(), 13);
        s.chat = TTF_OpenFont(body.c_str(), 12);
        s.tiny = TTF_OpenFont(body.c_str(), 10);
        if (s.tiny) TTF_SetFontStyle(s.tiny, TTF_STYLE_BOLD);
    }
    SettingsModernText& T = *settingsModern;
    T.used = 0;

    SDL_Renderer* rend = const_cast<SDL_Renderer*>(renderer);
    GameSettings* gs = GameSettings::Instance();
    PlayerKeys* allKeys[5] = {
        &gs->player1Keys, &gs->player2Keys, &gs->player3Keys,
        &gs->player4Keys, &gs->player5Keys
    };
    PlayerKeys& pk = *allKeys[keyConfigPlayer - 1];

    // The world map stays visible behind, dimmed, as in the game room.
    menulist::DrawWorldMapBackdrop(rend, netGameBackground);
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(rend, 5, 10, 22, 158);
    SDL_RenderFillRect(rend, nullptr);

    auto focusRow = [&](const SDL_Rect& r) {
        const SDL_FRect f = ToFRect(r);
        FillRoundRect(rend, f, 8, kRowFocus);
        StrokeRoundRect(rend, f, 8, 1.5f, kIce);
    };
    // A row's frame and label; returns its middle.
    auto row = [&](int index, const SDL_Rect& r, const char* label, SDL_Color fg) {
        const bool f = keyConfigIndex == index;
        if (f) focusRow(r);
        else FillRoundRect(rend, ToFRect(r), 8, kRowIdle);
        const int cy = r.y + r.h / 2;
        DrawTextLine(rend, T.Take(T.body), label, f ? kValue : fg, kNoShadow, r.x + 10, cy);
        return cy;
    };
    auto sectionLabel = [&](const char* s, int x, int y) {
        DrawTextLine(rend, T.Take(T.tiny), s, kLabel, kNoShadow, x, y);
    };

    // ---- Top bar ------------------------------------------------------------
    DrawCard(rend, kTopBar);
    const int tw = DrawTextLine(rend, T.Take(T.title), "SETTINGS", kValue, kTextShadow, 28, 32);
    DrawChip(rend, T.Take(T.tiny), APP_VERSION, 28 + tw + 12, 31, kLabel);
    int connected = 0;
    { SDL_JoystickID* joys = SDL_GetJoysticks(&connected); SDL_free(joys); }
    char ctrlText[40];
    snprintf(ctrlText, sizeof(ctrlText), connected == 1 ? "1 controller connected"
                                                        : "%d controllers connected", connected);
    DrawTextLine(rend, T.Take(T.chat), ctrlText, connected ? kIce : kLabel, kNoShadow, 612, 31, 2);

    // The Game card's rows are as tall as fits: 40 apart, closer on Android,
    // whose three store rows have to fit too.
    const int switchCount = 2
#ifndef __WASM_PORT__
        + 1
#endif
        ;
    int gameRows = 1 + switchCount + 3;   // speed, switches, How to play, Replays, Reset all
    int gameExtra = 20;                   // the MORE heading
#ifdef __ANDROID__
    const bool adsRemoved = AdsRemoved();
    gameRows += adsRemoved ? 1 : 3;
    gameExtra += adsRemoved ? 20 : 32;    // the STORE heading, the renewal line
#endif
    const int firstRowY = kCardTop + 26;
    const int pitch = std::min(40, (kCardBottomMax - 14 - firstRowY - gameExtra) / gameRows);
    const int rowH = pitch - 4;
    const int gameBottom = firstRowY + gameRows * pitch + gameExtra - 4 + 14;
    const int controlsBottom = kCardTop + 66 + (keyConfigPlayer == 1 ? 5 : 4) * kKeyPitch + 4 + 32 + 14;
    const int cardH = std::min(kCardBottomMax, std::max(gameBottom, controlsBottom)) - kCardTop;
    const SDL_Rect kControls = {kControlsX, kCardTop, kControlsW, cardH};
    const SDL_Rect kGame = {kGameX, kCardTop, kGameW, cardH};

    // ---- Controls: a tab per player, then that player's bindings ------------
    DrawCard(rend, kControls);
    sectionLabel("CONTROLS", kControls.x + 16, kControls.y + 14);
    const int innerX = kControls.x + 12, innerW = kControls.w - 24;
    const int tabW = (innerW - 3 * 6) / 4;
    for (int p = 1; p <= 4; p++) {
        const SDL_Rect tab = {innerX + (p - 1) * (tabW + 6), kControls.y + 28, tabW, 26};
        char label[12];
        snprintf(label, sizeof(label), "PLAYER %d", p);
        DrawPill(rend, T.Take(T.small), label, tab, p == keyConfigPlayer,
                 keyConfigIndex == kKeyPlayerTapBase + p);
        AddPanelTapRow(kKeyPlayerTapBase + p, tab, -1, false, SDLK_1 + (p - 1));
    }

    struct { int idx; const char* label; SDL_Scancode sc; } keyRows[5] = {
        {kKeyRowLeft,     "Turn left",    pk.left},
        {kKeyRowRight,    "Turn right",   pk.right},
        {kKeyRowFire,     "Fire",         pk.fire},
        {kKeyRowCenter,   "Center",       pk.center},
        {kKeyRowFireNext, kFireNextLabel, pk.fireNext},
    };
    int y = kControls.y + 66;
    for (auto& k : keyRows) {
        const SDL_Rect r = {innerX, y, innerW, kKeyH};
        const int cy = row(k.idx, r, k.label, kFrost);
        if (awaitKp && keyConfigIndex == k.idx) {
            DrawTextLine(rend, T.Take(T.value), "Press a key...", kGold, kNoShadow, r.x + r.w - 12, cy, 2);
        } else {
            // The binding as a key cap.
            const std::string name = ControllerScancodeName(k.sc);
            int w = 0;
            if (T.value) TTF_GetStringSize(T.value, name.c_str(), 0, &w, nullptr);
            const SDL_FRect cap = {(float)(r.x + r.w - 32 - w), (float)(cy - 11), (float)(w + 20), 22};
            FillRoundRect(rend, cap, 6, {127, 214, 255, 20});
            StrokeRoundRect(rend, cap, 6, 1, kEdge);
            DrawTextLine(rend, T.Take(T.value), name.c_str(), kValue, kNoShadow, r.x + r.w - 22, cy, 2);
        }
        AddPanelTapRow(k.idx, r);
        y += kKeyPitch;
    }

    const SDL_Rect reset = {innerX, y + 4, innerW, 32};
    DrawPill(rend, T.Take(T.small), "Reset to defaults", reset, false, keyConfigIndex == kKeyRowResetCtrl);
    AddPanelTapRow(kKeyRowResetCtrl, reset);

    // ---- Game ------------------------------------------------------------------
    DrawCard(rend, kGame);
    const int gx = kGame.x + 12, gw = kGame.w - 24;
    sectionLabel("GAME", kGame.x + 16, kGame.y + 14);
    y = firstRowY;

    {
        // Game speed: LEFT/RIGHT step it, so its tap rows split at the value
        // (menulist::List::End), the label and left half stepping down.
        const SDL_Rect r = {gx, y, gw, rowH};
        const bool f = keyConfigIndex == kKeyRowSpeed;
        const int cy = row(kKeyRowSpeed, r, "Game speed", kFrost);
        char speed[12];
        snprintf(speed, sizeof(speed), "%.1fx", gs->speedMultiplier);
        const int xr = r.x + r.w - 12;
        const int right = f ? xr - 12 : xr;
        const int w = DrawTextLine(rend, T.Take(T.value), speed, f ? kValue : kFrost, kTextShadow, right, cy, 2);
        if (f) {
            DrawTextLine(rend, T.Take(T.body), "\xE2\x80\xB9", kIce, kNoShadow, right - w - 10, cy - 1, 1);
            DrawTextLine(rend, T.Take(T.body), "\xE2\x80\xBA", kIce, kNoShadow, xr - 2, cy - 1, 1);
        }
        const int split = (right - w - 18 + r.x + r.w) / 2;
        AddPanelTapRow(kKeyRowSpeed, {r.x, r.y, split - r.x, r.h}, -1, false, SDLK_LEFT);
        AddPanelTapRow(kKeyRowSpeed, {split, r.y, r.x + r.w - split, r.h}, -1, false, SDLK_RIGHT);
        y += pitch;
    }

    struct { int idx; const char* label; bool on; } switches[] = {
        {kKeyRowSound, "Sound", gs->soundEnabled()},
        {kKeyRowMouse, "Mouse / touch aim", gs->mouseEnabled},
#ifndef __WASM_PORT__
        {kKeyRowFullscreen, "Fullscreen", gs->fullscreenMode()},
#endif
    };
    for (auto& s : switches) {
        const SDL_Rect r = {gx, y, gw, rowH};
        const int cy = row(s.idx, r, s.label, kFrost);
        DrawSwitch(rend, r.x + r.w - 42, cy, s.on);
        AddPanelTapRow(s.idx, r);
        y += pitch;
    }

#ifdef __ANDROID__
    // Ad removal, as on the classic panel: Play's own price once it has
    // arrived, and the renewal terms under a focused subscription (Play policy
    // wants them shown before purchase).
    y += 8;
    sectionLabel("STORE", kGame.x + 16, y);
    y += 12;
    if (adsRemoved) {
        DrawTextLine(rend, T.Take(T.body), "Ads removed -- thank you!", kGold, kNoShadow, gx + 10, y + rowH / 2);
        y += pitch;
    } else {
        struct { int idx; const char* label; std::string price; } ads[3] = {
            {kKeyRowRemoveAdsMonth, "Remove ads (1 month)", AdsPrice(0)},
            {kKeyRowRemoveAdsYear, "Remove ads (1 year)", AdsPrice(1)},
            {kKeyRowRemoveAdsForever, "Remove ads (forever)", AdsPrice(2)},
        };
        if (!ads[0].price.empty()) ads[0].price += "/mo";
        if (!ads[1].price.empty()) ads[1].price += "/yr";
        for (auto& a : ads) {
            const SDL_Rect r = {gx, y, gw, rowH};
            const int cy = row(a.idx, r, a.label, kFrost);
            DrawTextLine(rend, T.Take(T.value), a.price.empty() ? "..." : a.price.c_str(), kGold, kNoShadow,
                         r.x + r.w - 12, cy, 2);
            AddPanelTapRow(a.idx, r);
            y += pitch;
        }
        const char* terms = keyConfigIndex == kKeyRowRemoveAdsMonth ? "Renews monthly, cancel in Play"
                          : keyConfigIndex == kKeyRowRemoveAdsYear  ? "Renews yearly, cancel in Play"
                                                                    : nullptr;
        if (terms) DrawTextLine(rend, T.Take(T.chat), terms, kLabel, kNoShadow, gx + 10, y + 4);
        y += 12;
    }
#endif

    y += 8;
    sectionLabel("MORE", kGame.x + 16, y);
    y += 12;
    {
        const SDL_Rect r = {gx, y, gw, rowH};
        const int cy = row(kKeyRowHowTo, r, "How to play", kFrost);
        DrawTextLine(rend, T.Take(T.chat), "swap, aim, shoot", kLabel, kNoShadow, r.x + r.w - 24, cy, 2);
        DrawTextLine(rend, T.Take(T.body), "\xE2\x80\xBA", kIce, kNoShadow, r.x + r.w - 12, cy - 1, 1);
        AddPanelTapRow(kKeyRowHowTo, r);
        y += pitch;
    }
    {
        const SDL_Rect r = {gx, y, gw, rowH};
        const int cy = row(kKeyRowReplays, r, "Replays", kFrost);
        DrawTextLine(rend, T.Take(T.chat), "play / manage", kLabel, kNoShadow, r.x + r.w - 24, cy, 2);
        DrawTextLine(rend, T.Take(T.body), "\xE2\x80\xBA", kIce, kNoShadow, r.x + r.w - 12, cy - 1, 1);
        AddPanelTapRow(kKeyRowReplays, r);
        y += pitch;
    }
    {
        // Two presses, as on the classic panel: the first arms it.
        const SDL_Rect r = {gx, y, gw, rowH};
        const bool armed = resetAllArmed && keyConfigIndex == kKeyRowResetAll;
        const int cy = row(kKeyRowResetAll, r, "Reset all settings", kLose);
        if (armed)
            DrawTextLine(rend, T.Take(T.small), "PRESS AGAIN TO CONFIRM", kLose, kNoShadow, r.x + r.w - 12, cy, 2);
        AddPanelTapRow(kKeyRowResetAll, r);
    }

    menulist::DrawFooterHint(rend, panelText,
        awaitKp ? "Press a button or key..."
                : "UP/DOWN select    ENTER change    LEFT/RIGHT or 1-4 player    ESC done");
}
