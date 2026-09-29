#include "mainmenu.h"
#include "networkclient.h"
#include "menulist.h"
#include "sdl3_compat.h"
#include "audiomixer.h"
#include <algorithm>
#include <ctime>

// The online lobby's "Weekly rankings" screen: the connected server's top
// players this week (Monday 00:00 UTC onward) by round wins, round losses and
// bubbles popped, plus the player's own line. Full-screen like the tournament
// view it sits beside, and reached the same way -- a row in the lobby
// (LobbyWeeklyIndex()). Data comes from fb-server's WEEKLY command
// (NetworkClient::RequestWeekly()), fetched on open and on Refresh.

namespace {
enum WeeklyButton { kWeeklyRefresh = 0, kWeeklyBack = 1, kWeeklyButtonCount = 2 };
}

int MainMenu::LobbyWeeklyIndex(size_t roomCount) const {
    // Always present, even against a server too old to answer WEEKLY -- the
    // screen says so itself. Keeping the row unconditional keeps every index
    // after it (the Discord row) fixed regardless of which server this is.
    return LobbyRoomListStart() + (int)roomCount;
}

void MainMenu::OpenWeekly() {
    showingWeekly = true;
    weeklySelection = kWeeklyBack;
    SDL_StopTextInput(SDL_GetKeyboardFocus());
    NetworkClient::Instance()->RequestWeekly();
}

void MainMenu::WeeklyPanelRender() {
    auto* net = NetworkClient::Instance();
    if (!net->IsConnected()) { showingWeekly = false; return; }
    auto* rend = const_cast<SDL_Renderer*>(renderer);
    SDL_SetRenderDrawColor(rend, 17, 26, 45, 255);
    SDL_RenderClear(rend);
    panelText.UpdateStyle(13, TTF_STYLE_NORMAL);
    auto text = [&](const std::string& label, int x, int y, SDL_Color color = menulist::kText) {
        panelText.UpdateColor(color, menulist::kTextShadow);
        panelText.UpdateText(rend, label.c_str(), 0);
        panelText.UpdatePosition({x, y});
        SDL_FRect r = ToFRect(*panelText.Coords());
        SDL_RenderTexture(rend, panelText.Texture(), nullptr, &r);
    };
    auto textRight = [&](const std::string& label, int right, int y, SDL_Color color) {
        panelText.UpdateText(rend, label.c_str(), 0);
        text(label, right - panelText.Coords()->w, y, color);
    };

    text("WEEKLY RANKINGS", 18, 14, menulist::kGold);

    const WeeklyBoard& b = net->weekly;
    if (!net->WeeklySupported()) {
        text("This server doesn't have weekly rankings yet.", 18, 74);
        text("They appear once its operator updates fb-server.", 18, 98, menulist::kMuted);
    } else if (!net->weeklyLoaded) {
        text("Loading...", 18, 74, menulist::kMuted);
    } else {
        char sub[128];
        // Plain gmtime(): its shared static buffer is fine on the UI thread,
        // and it avoids gmtime_r/gmtime_s, which aren't both available on
        // every platform this builds for (MinGW declares neither by default).
        const time_t ws = (time_t)b.weekStart;
        char day[32] = "";
        if (const struct tm* tmv = gmtime(&ws))
            strftime(day, sizeof(day), "%b %d", tmv);
        snprintf(sub, sizeof(sub), "Week of %s  ·  resets Monday 00:00 UTC  ·  bots not counted", day);
        text(sub, 18, 40, menulist::kMuted);

        // A 1.6 server lists signed-in accounts as "nick#tag"; an older one,
        // or a connection that isn't signed in, lists bare nicks.
        const std::string me = net->accountId.empty()
            ? net->GetPlayerNick() : net->GetPlayerNick() + "#" + net->AccountTag();
        struct Column { const char* title; const std::vector<std::pair<std::string, int>>* list; };
        const Column cols[3] = {{"Round wins", &b.wins}, {"Round losses", &b.losses},
                                {"Bubbles popped", &b.popped}};
        for (int c = 0; c < 3; ++c) {
            const int x = 18 + c * 208, w = 196;
            SDL_SetRenderDrawColor(rend, 31, 47, 70, 255);
            SDL_FRect box{(float)x - 6, 68.f, (float)w, 272.f};
            SDL_RenderFillRect(rend, &box);
            text(cols[c].title, x, 72, menulist::kGold);
            const auto& list = *cols[c].list;
            if (list.empty()) text("Nobody yet", x, 100, menulist::kMuted);
            for (size_t i = 0; i < list.size() && i < 10; ++i) {
                const int y = 100 + (int)i * 23;
                const SDL_Color col = list[i].first == me ? menulist::kGold : menulist::kText;
                char left[48];
                snprintf(left, sizeof(left), "%2d. %s", WeeklyBoard::Rank(list, i), list[i].first.c_str());
                text(left, x, y, col);
                textRight(std::to_string(list[i].second), x + w - 14, y, col);
            }
        }

        char mine[160];
        auto rank = [](int r) { return r > 0 ? "#" + std::to_string(r) : std::string("-"); };
        if (b.hasMine) {
            snprintf(mine, sizeof(mine), "You: %d wins (%s)  ·  %d losses (%s)  ·  %d popped (%s)",
                     b.myWins, rank(b.myWinsRank).c_str(), b.myLosses, rank(b.myLossesRank).c_str(),
                     b.myPopped, rank(b.myPoppedRank).c_str());
        } else {
            snprintf(mine, sizeof(mine), "You haven't finished a round this week yet.");
        }
        text(mine, 18, 352, b.hasMine ? menulist::kGold : menulist::kMuted);
    }

    // Two buttons, keyboard focus drawn as the highlighted one (CLAUDE.md's
    // input-parity checklist: a choice between several must show which is
    // focused, not leave it to be inferred).
    static const char* kLabels[kWeeklyButtonCount] = {"Refresh", "Back to lobby"};
    weeklySelection = std::clamp(weeklySelection, 0, kWeeklyButtonCount - 1);
    BeginPanelTapRows(&weeklySelection);
    for (int i = 0; i < kWeeklyButtonCount; ++i) {
        SDL_Rect r = {18 + i * 153, 396, 146, 26};
        const bool sel = i == weeklySelection;
        SDL_SetRenderDrawColor(rend, sel ? 94 : 35, 69, 76, 255);
        auto fr = ToFRect(r);
        SDL_RenderFillRect(rend, &fr);
        text(kLabels[i], r.x + 7, r.y + 3, sel ? menulist::kGold : menulist::kText);
        AddPanelTapRow(i, r);
    }
    menulist::DrawFooterHint(rend, panelText, "LEFT/RIGHT: MOVE   ENTER / A: SELECT   ESC / B: BACK");
    panelText.UpdateStyle(15, TTF_STYLE_NORMAL);
}

bool MainMenu::WeeklyPanelKey(SDL_Event* e) {
    if (!showingWeekly || e->type != SDL_EVENT_KEY_DOWN) return false;
    const auto key = e->key.key;
    if (key == SDLK_ESCAPE) {
        showingWeekly = false;
    } else if (key == SDLK_LEFT || key == SDLK_UP) {
        weeklySelection = (weeklySelection + kWeeklyButtonCount - 1) % kWeeklyButtonCount;
    } else if (key == SDLK_RIGHT || key == SDLK_DOWN || key == SDLK_TAB) {
        weeklySelection = (weeklySelection + 1) % kWeeklyButtonCount;
    } else if (key == SDLK_RETURN || key == SDLK_SPACE) {
        AudioMixer::Instance()->PlaySFX("menu_selected");
        if (weeklySelection == kWeeklyRefresh) NetworkClient::Instance()->RequestWeekly();
        else showingWeekly = false;
    }
    return true;
}
