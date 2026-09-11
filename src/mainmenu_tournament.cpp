#include "mainmenu.h"
#include "networkclient.h"
#include "menulist.h"
#include "sdl3_compat.h"
#include "platform.h"
#include <algorithm>

void MainMenu::OpenTournament(int id) {
    showingTournament = true;
    tournamentViewId = id;
    tournamentSelection = tournamentSection = 0;
    tournamentConfirm = false;
    tournamentButtons.clear();
    SDL_StopTextInput(SDL_GetKeyboardFocus());
    NetworkClient* net = NetworkClient::Instance();
    net->TournamentCommand(id ? "WATCH " + std::to_string(id) : "LIST");
}

int MainMenu::LobbyTournamentIndex() const {
    // Fixed at slot 2, right under Create Game Room (2026-09-11 relayout --
    // it used to trail the room list and Discord row in the right sidebar,
    // but the organizer feature belongs beside the other room-management
    // action, not sharing a scrolling list with the "Online" player sidebar
    // that had nothing to do with it).
    auto* net = NetworkClient::Existing();
    return net && net->tournaments.supported ? 2 : -1;
}

int MainMenu::LobbyRoomListStart() const {
    return LobbyTournamentIndex() >= 0 ? 3 : 2;
}

void MainMenu::TournamentPanelRender() {
    auto* net = NetworkClient::Instance();
    if (!net->IsConnected()) { showingTournament = false; return; }
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
    const auto* s = net->tournaments.Find(tournamentViewId);
    // A create/join response brings its participant to the newly received view.
    if (!tournamentViewId && net->tournaments.ActiveId()) {
        tournamentViewId = net->tournaments.ActiveId();
        s = net->tournaments.Find(tournamentViewId);
    }
    tournamentButtons.clear();
    text(tournamentConfiguring ? "CREATE TOURNAMENT" : s ? "TOURNAMENT #" + std::to_string(s->id) : "ONLINE TOURNAMENTS", 18, 14, menulist::kGold);
    if (tournamentConfiguring) {
        text("Choose a ruleset -- locked in for every match once created", 18, 45);
        char modeText[48], malusText[48], chainText[48], aimText[48], colorsText[48];
        snprintf(modeText, sizeof(modeText), "Game mode: %s", GameModeName(netGameMode));
        snprintf(malusText, sizeof(malusText), "Attack bubbles: %s", AttackModeName(netAttackMode));
        snprintf(chainText, sizeof(chainText), "Chain reaction: %s", chainReactionEnabled ? "ON" : "OFF");
        snprintf(aimText, sizeof(aimText), "Aim guide: %s", tournamentAimGuide ? "ON" : "OFF");
        snprintf(colorsText, sizeof(colorsText), "Colors: %d", tournamentColors);
        tournamentButtons.push_back({modeText, "CFG_MODE"});
        tournamentButtons.push_back({malusText, "CFG_ATTACK"});
        tournamentButtons.push_back({chainText, "CFG_CHAIN"});
        tournamentButtons.push_back({aimText, "CFG_AIM"});
        tournamentButtons.push_back({colorsText, "CFG_COLORS"});
        tournamentButtons.push_back({"Create tournament", "CFG_CONFIRM"});
        tournamentButtons.push_back({"Back", "CFG_BACK"});
    } else if (!s) {
        text("Single elimination · 4–16 players · First to 2 wins", 18, 45);
        tournamentButtons.push_back({"Create tournament", "CONFIGURE"});
        tournamentButtons.push_back({"Refresh", "LIST"});
        for (const auto& item : net->tournaments.list)
            tournamentButtons.push_back({item.owner + " · " + std::to_string(item.count) + " · " + item.state, "VIEW " + std::to_string(item.id)});
        tournamentButtons.push_back({"Back to lobby", "CLOSE"});
    } else {
        std::string status = s->state;
        if (s->champion) status = "Champion: " + s->Name(s->champion);
        else if (const auto* mine = s->MyMatch()) {
            int remaining = mine->remaining;
            auto when = net->tournamentReceivedAt.find(s->id);
            if (when != net->tournamentReceivedAt.end()) remaining = std::max(0, remaining - static_cast<int>((SDL_GetTicks() - when->second) / 1000));
            status = "Your match: " + mine->state;
            if (remaining) status += " · " + std::to_string(remaining) + "s";
            if (mine->state == "disputed") status += " · Awaiting operator";
        } else if (s->state == "running") status = "Following bracket · Waiting for this stage";
        text(status, 18, 45, menulist::kGold);
        if (s->state == "registration") {
            text(std::to_string(s->entrants.size()) + " / 16 entrants · Minimum 4 · Organizer starts when ready", 18, 74);
            for (size_t i = 0; i < s->entrants.size(); ++i) {
                const auto& e = s->entrants[i];
                text(e.nick + (e.id == s->owner ? " (organizer)" : ""),
                     22 + static_cast<int>(i / 8) * 308, 105 + static_cast<int>(i % 8) * 27,
                     e.id == s->self ? menulist::kGold : menulist::kText);
            }
        } else {
            auto card = [&](const TournamentMatch& m, int x, int y, int w) {
                const bool mine = s->self && (m.a == s->self || m.b == s->self);
                SDL_SetRenderDrawColor(rend, mine ? 75 : 31, mine ? 60 : 47, 70, 255);
                SDL_FRect r{static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), 61};
                SDL_RenderFillRect(rend, &r);
                SDL_SetRenderDrawColor(rend, mine ? 255 : 83, mine ? 218 : 117, 92, 255);
                SDL_RenderRect(rend, &r);
                text(s->Name(m.a) + " " + std::to_string(m.winsA) + " : " + std::to_string(m.winsB) + " " + s->Name(m.b), x + 7, y + 5, mine ? menulist::kGold : menulist::kText);
                text("#" + std::to_string(m.id) + " · " + m.state, x + 7, y + 29, menulist::kMuted);
            };
            if (s->matches.size() <= 7) {
                // 4-slot brackets (3 matches: Semifinals, Final) and 8-slot
                // brackets (7 matches: Quarterfinals, Semifinals, Final)
                // share this compact column layout; only the number of
                // stages and each stage's opening match count differ, both
                // derived from matches.size() (== slots-1) rather than
                // assuming 8 slots outright.
                const int numStages = s->matches.size() == 3 ? 2 : 3;
                const char* labels3[] = {"Quarterfinals", "Semifinals", "Final"};
                const char* labels2[] = {"Semifinals", "Final"};
                const char* const* labels = numStages == 3 ? labels3 : labels2;
                for (int stage = 0; stage < numStages; ++stage) {
                    text(labels[stage], 18 + stage * 207, 81, menulist::kMuted);
                    int row = 0, count = (static_cast<int>(s->matches.size()) + 1) / 2 >> stage;
                    for (const auto& m : s->matches) if (m.stage == stage) {
                        const int y = 108 + (row++ * 2 + 1) * 264 / (count * 2) - 30;
                        card(m, 12 + stage * 207, y, 198);
                        if (stage < numStages - 1) {
                            SDL_SetRenderDrawColor(rend, 83, 117, 140, 255);
                            SDL_RenderLine(rend, 210 + stage * 207, y + 30, 219 + stage * 207, y + 30);
                        }
                    }
                }
            } else {
                // Five sections preserve readable names on the 640×480 canvas.
                const int section = tournamentSection % 5;
                const int stage = section < 2 ? 0 : section - 1;
                const int start = section == 1 ? 4 : 0;
                const char* labels[] = {"Round of 16 · Matches 1–4", "Round of 16 · Matches 5–8", "Quarterfinals", "Semifinals", "Final"};
                text(labels[section], 18, 81, menulist::kMuted);
                int index = 0, row = 0;
                for (const auto& m : s->matches) if (m.stage == stage) {
                    if (index++ < start || row >= 4) continue;
                    card(m, 18 + (row % 2) * 306, 122 + (row / 2) * 109, 291);
                    if (stage > 0) text("Winners advance from the previous section", 18, 350, menulist::kMuted);
                    ++row;
                }
            }
        }
        if (s->state == "registration") {
            text("Classic · 8 colors · Compression on · Aim guide off", 18, 338, menulist::kMuted);
            text("Mouse/touch on · Leaving or disconnecting forfeits", 18, 362, menulist::kMuted);
        }
        tournamentButtons = TournamentActions(s, net->tournaments.ActiveId() && net->tournaments.ActiveId() != s->id);
    }
    if (tournamentConfirm) {
        SDL_SetRenderDrawColor(rend, 17, 26, 45, 255);
        SDL_FRect overlay{8, 90, 624, 354}; SDL_RenderFillRect(rend, &overlay);
        text("Withdraw from this tournament?", 100, 180, menulist::kGold);
        text("An active match will be forfeited.", 100, 214);
        tournamentButtons = {{"Stay", "STAY"}, {"Withdraw", "LEAVE " + std::to_string(tournamentViewId)}};
    }
    tournamentSelection = std::clamp(tournamentSelection, 0, std::max(0, static_cast<int>(tournamentButtons.size()) - 1));
    BeginPanelTapRows(&tournamentSelection);
    for (size_t i = 0; i < tournamentButtons.size(); ++i) {
        SDL_Rect r;
        if (!s && !tournamentConfirm) r = {18, 87 + static_cast<int>(i) * 30, 604, 27};
        else r = {18 + static_cast<int>(i % 4) * 153, 396 + static_cast<int>(i / 4) * 29, 146, 26};
        SDL_SetRenderDrawColor(rend, i == static_cast<size_t>(tournamentSelection) ? 94 : 35, 69, 76, 255);
        auto fr = ToFRect(r); SDL_RenderFillRect(rend, &fr);
        text(tournamentButtons[i].label, r.x + 7, r.y + 3, i == static_cast<size_t>(tournamentSelection) ? menulist::kGold : menulist::kText);
        AddPanelTapRow(static_cast<int>(i), r);
    }
    if (!net->tournamentError.empty()) {
        std::string error = net->tournamentError;
        std::replace(error.begin(), error.end(), '_', ' ');
        text(error, 18, 429, menulist::kBad);
    }
    menulist::DrawFooterHint(rend, panelText, "ARROWS / D-PAD: MOVE   ENTER / A: SELECT   ESC / B: BACK");
    panelText.UpdateStyle(15, TTF_STYLE_NORMAL);
}

bool MainMenu::TournamentPanelKey(SDL_Event* e) {
    if (!showingTournament || e->type != SDL_EVENT_KEY_DOWN) return false;
    const auto key = e->key.key;
    const int count = static_cast<int>(tournamentButtons.size());
    if (key == SDLK_ESCAPE) {
        if (tournamentConfirm) { tournamentConfirm = false; tournamentSelection = 0; }
        else if (tournamentConfiguring) { tournamentConfiguring = false; tournamentSelection = 0; }
        else if (tournamentViewId) { tournamentViewId = 0; showingTournament = false; }
        else showingTournament = false;
        return true;
    }
    if ((key == SDLK_UP || key == SDLK_LEFT) && count) tournamentSelection = (tournamentSelection + count - 1) % count;
    else if ((key == SDLK_DOWN || key == SDLK_RIGHT || key == SDLK_TAB) && count) tournamentSelection = (tournamentSelection + 1) % count;
    else if ((key == SDLK_RETURN || key == SDLK_SPACE) && count) {
        auto* net = NetworkClient::Instance();
        const auto command = tournamentButtons[std::clamp(tournamentSelection, 0, count - 1)].command;
        if (command == "CLOSE" || command == "BACK") { showingTournament = false; }
        else if (command == "SECTION") { ++tournamentSection; }
        else if (command == "STAY") { tournamentConfirm = false; tournamentSelection = 0; }
        else if (command.compare(0, 9, "WITHDRAW ") == 0) { tournamentConfirm = true; tournamentSelection = 0; }
        else if (command.compare(0, 5, "VIEW ") == 0) OpenTournament(std::stoi(command.substr(5)));
        // "Create tournament" on the browse list opens the ruleset screen
        // instead of creating immediately -- CFG_CONFIRM below is what
        // actually sends TOUR CREATE, once the organizer has had a chance
        // to look at (and change) what they're locking in. Values start
        // from whatever this device's own room settings currently hold
        // (SyncRoomOptions'/mainmenu.cpp's usual "restore last-used
        // settings" load already ran when the net panel opened), same as a
        // freshly created room would.
        else if (command == "CONFIGURE") {
            tournamentConfiguring = true; tournamentSelection = 0;
            tournamentColors = playerColorCounts[0];
            tournamentAimGuide = playerAimGuide[0];
        }
        else if (command == "CFG_BACK") { tournamentConfiguring = false; tournamentSelection = 0; }
        else if (command == "CFG_MODE") { StepNetGameMode(true); }
        else if (command == "CFG_ATTACK") { netAttackMode = NextAttackMode(netAttackMode); }
        else if (command == "CFG_CHAIN") { chainReactionEnabled = !chainReactionEnabled; }
        else if (command == "CFG_AIM") { tournamentAimGuide = !tournamentAimGuide; }
        else if (command == "CFG_COLORS") { if (++tournamentColors > 8) tournamentColors = 2; }
        else if (command == "CFG_CONFIRM") {
            // Every slot gets the same value: a tournament match is always
            // exactly two entrants (P1/P2), and mirroring across all five
            // keeps this one BuildOptionsBlob call identical to the one a
            // live room's SyncRoomOptions() already makes, rather than a
            // second, tournament-only encoding to keep in sync with it.
            const int colors[5] = {tournamentColors, tournamentColors, tournamentColors, tournamentColors, tournamentColors};
            const bool noCompress[5] = {false, false, false, false, false};
            const bool aim[5] = {tournamentAimGuide, tournamentAimGuide, tournamentAimGuide, tournamentAimGuide, tournamentAimGuide};
            const int teams[5] = {0, 0, 0, 0, 0};
            std::string blob = NetworkClient::BuildOptionsBlob(chainReactionEnabled, /*continueWhenLeave=*/true,
                /*singleTarget=*/false, /*victoriesLimit=*/5, colors, noCompress, aim, /*mouseEnabled=*/false,
                netGameMode, RaceTargetAt(netRaceTargetIndex), TimedSecondsAt(netTimedSecondsIndex),
                netAttackMode, teams, /*teamCount=*/2);
            net->TournamentCommand("CREATE " + blob);
            SaveHostDefaults();
            tournamentConfiguring = false; tournamentSelection = 0;
        }
        else {
            net->TournamentCommand(command);
            if (command.compare(0, 6, "LEAVE ") == 0) { tournamentConfirm = false; tournamentSelection = 0; }
        }
    }
    return true;
}
