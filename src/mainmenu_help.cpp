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

// The full-page settings reference behind the HELP box on the online game
// room and the local multiplayer panel.
//
// Two pages, one frame: the scrolling, key handling and layout are shared, the
// prose is not. The online page documents the rows
// NetPanelLobbyActionsRender draws plus the rules only a network game reaches
// (solo targetting, the >5-alive royale target); the local page documents the
// rows LocalMPPanelRender draws and the rules that differ locally -- above
// all that a local attack is sent UNDIVIDED to every opponent, the opposite
// of the online split. Sharing one page between them would state that rule
// wrongly for one screen or the other, which is worse than having two.

#include "mainmenu_internal.h"
#include "menulist.h"
#include "attackmode.h"

#include <algorithm>

namespace {

// One line of the page. An empty `text` is a blank spacer; `heading` is drawn
// gold and bold with a rule under it, `bullet` gets an indent and a marker,
// and anything else is body text wrapped by hand at the widths below (the
// page is a fixed 640x480 canvas, so hand-wrapping keeps the line breaks
// where they read best rather than wherever a measured wrap happens to land).
struct HelpLine {
    enum Kind { Blank, Heading, Body, Bullet, Note } kind;
    const char* text;
};

const HelpLine kOnlineHelp[] = {
    {HelpLine::Heading, "ATTACK BUBBLES"},
    {HelpLine::Body,    "Pop more than two bubbles at once and the surplus is"},
    {HelpLine::Body,    "sent at your opponents as attack bubbles (malus)."},
    {HelpLine::Body,    "The count is: bubbles popped, plus any left hanging"},
    {HelpLine::Body,    "that then fall, minus two."},
    {HelpLine::Blank,   ""},
    {HelpLine::Bullet,  "ON  -  everything you earn is sent straight out,"},
    {HelpLine::Body,    "    and everything sent at you lands."},
    {HelpLine::Bullet,  "OFF  -  nobody attacks anybody. A pure race."},
    {HelpLine::Bullet,  "Blockable  -  what you earn first pays down what is"},
    {HelpLine::Body,    "    still queued against you, and only the surplus"},
    {HelpLine::Body,    "    is sent on."},
    {HelpLine::Blank,   ""},
    {HelpLine::Note,    "Blocking never goes negative. Blocking more than you"},
    {HelpLine::Note,    "owe just empties your queue and sends nothing - it"},
    {HelpLine::Note,    "does not bank credit against the next wave. Only"},
    {HelpLine::Note,    "bubbles still queued can be blocked; once one has"},
    {HelpLine::Note,    "started falling it is committed and will land. A"},
    {HelpLine::Note,    "\"Blocked -N\" toast shows when a block happens."},
    {HelpLine::Blank,   ""},

    {HelpLine::Heading, "WHO YOUR ATTACK HITS"},
    {HelpLine::Body,    "With 5 or fewer players still alive, an attack is"},
    {HelpLine::Body,    "split evenly among every living opponent, rounding"},
    {HelpLine::Body,    "up - so each of them gets the same share."},
    {HelpLine::Blank,   ""},
    {HelpLine::Body,    "With 6 or more still alive the rule changes. Splitting"},
    {HelpLine::Body,    "would multiply one pop into an attack on nineteen"},
    {HelpLine::Body,    "boards at once, so instead the whole attack goes to a"},
    {HelpLine::Body,    "SINGLE opponent, Tetris-99 style: whoever you have"},
    {HelpLine::Body,    "targeted, or a random living player if you have not"},
    {HelpLine::Body,    "targeted anyone."},
    {HelpLine::Blank,   ""},
    {HelpLine::Note,    "This is judged on how many are ALIVE right now, not"},
    {HelpLine::Note,    "the room size. A 10-player room that has thinned to 5"},
    {HelpLine::Note,    "survivors goes back to splitting mid-match."},
    {HelpLine::Note,    "Solo targetting below only affects the 5-or-fewer"},
    {HelpLine::Note,    "case; the 6-plus rule ignores it entirely."},
    {HelpLine::Blank,   ""},

    {HelpLine::Heading, "HOW ATTACKS LAND"},
    {HelpLine::Body,    "Attack bubbles wait briefly, then drop into your board"},
    {HelpLine::Body,    "in a wave. Each one picks a column at random and"},
    {HelpLine::Body,    "parks one row under whatever it meets there, so a"},
    {HelpLine::Body,    "single wave can hit the same column more than once."},
    {HelpLine::Body,    "At most seven fall at a time; the rest keep queuing."},
    {HelpLine::Blank,   ""},

    {HelpLine::Heading, "GAME MODE"},
    {HelpLine::Bullet,  "Classic  -  survive; you lose when your board reaches"},
    {HelpLine::Body,    "    the bottom line. Last one standing wins."},
    {HelpLine::Bullet,  "Clear  -  race to empty your board. Forces row"},
    {HelpLine::Body,    "    collapse and attack bubbles off while it is on,"},
    {HelpLine::Body,    "    and restores your old choices when switched off."},
    {HelpLine::Bullet,  "Teams  -  attacks skip your own team. Set each"},
    {HelpLine::Body,    "    player's team in the roster."},
    {HelpLine::Blank,   ""},

    {HelpLine::Heading, "CHAIN REACTION"},
    {HelpLine::Body,    "When on, bubbles knocked loose by a pop travel to an"},
    {HelpLine::Body,    "opponent's board on the way down instead of simply"},
    {HelpLine::Body,    "falling away. Off is the quieter, more predictable"},
    {HelpLine::Body,    "game; it does not change the attack count itself."},
    {HelpLine::Blank,   ""},

    {HelpLine::Heading, "SOLO TARGETTING"},
    {HelpLine::Body,    "Lets you aim your attacks at one chosen opponent"},
    {HelpLine::Body,    "instead of splitting them. Pick the target on your"},
    {HelpLine::Body,    "own screen during the game; with nobody picked,"},
    {HelpLine::Body,    "attacks split as usual. Ignored in Teams mode, and"},
    {HelpLine::Body,    "ignored while 6 or more players are alive."},
    {HelpLine::Blank,   ""},

    {HelpLine::Heading, "VICTORIES LIMIT"},
    {HelpLine::Body,    "How many rounds someone must win to take the match."},
    {HelpLine::Body,    "\"none\" plays on until everybody leaves."},
    {HelpLine::Blank,   ""},

    {HelpLine::Heading, "MOUSE / TOUCH AIM"},
    {HelpLine::Body,    "Aim and fire by pointing at the board instead of using"},
    {HelpLine::Body,    "the keys. Per player, and remembered between sessions."},
    {HelpLine::Blank,   ""},

    {HelpLine::Heading, "PER-PLAYER GRID"},
    {HelpLine::Bullet,  "Max colors  -  5 to 8. Fewer colors means easier"},
    {HelpLine::Body,    "    matches and faster pops."},
    {HelpLine::Bullet,  "Rows  -  off stops the board dropping a new row when"},
    {HelpLine::Body,    "    you go too long without a match."},
    {HelpLine::Bullet,  "Aim  -  draws the bounce line your shot will follow."},
    {HelpLine::Bullet,  "Team  -  which side that player is on in Teams mode."},
    {HelpLine::Blank,   ""},

    {HelpLine::Heading, "BOTS"},
    {HelpLine::Body,    "The host can fill empty seats with bots. A bot joins"},
    {HelpLine::Body,    "the room as an ordinary member - everyone sees it in"},
    {HelpLine::Body,    "the roster and it counts against the room cap - but"},
    {HelpLine::Body,    "only the host's machine plays it."},
    {HelpLine::Blank,   ""},
    {HelpLine::Body,    "Bot skill sets how hard they try: Low, Med or High."},
    {HelpLine::Body,    "A stronger bot also reacts faster, not just more"},
    {HelpLine::Body,    "accurately. Each bot keeps the skill it was added at,"},
    {HelpLine::Body,    "so changing the setting only affects the next one -"},
    {HelpLine::Body,    "which is how you get a mixed field."},
    {HelpLine::Blank,   ""},

    {HelpLine::Heading, "IN THE ROOM"},
    {HelpLine::Body,    "Only the host can change these settings; everyone else"},
    {HelpLine::Body,    "sees them update live. ESC leaves the room."},
};

// Local multiplayer's own page. Deliberately not the online one with the
// network parts removed: the settings list is different, and the two rules
// that matter most here -- attacks are NOT divided between opponents, and the
// seat count is capped at five -- are the opposite of what the online page
// says, so a player reading the wrong one would be misinformed rather than
// merely under-informed.
const HelpLine kLocalHelp[] = {
    {HelpLine::Heading, "PLAYERS AND CONTROLS"},
    {HelpLine::Body,    "Two to five players share this machine. Player 1 is"},
    {HelpLine::Body,    "always a person; bots fill the last seats, so you can"},
    {HelpLine::Body,    "never end up with a game of nothing but bots."},
    {HelpLine::Blank,   ""},
    {HelpLine::Body,    "Each human seat needs its own keyboard set or"},
    {HelpLine::Body,    "controller. The Players panel on the right shows what"},
    {HelpLine::Body,    "each seat is bound to, and warns when there are fewer"},
    {HelpLine::Body,    "controllers plugged in than humans to drive them."},
    {HelpLine::Blank,   ""},
    {HelpLine::Note,    "Five is the engine's ceiling here - one full board in"},
    {HelpLine::Note,    "the centre and four smaller ones in the corners. For"},
    {HelpLine::Note,    "more players than that you need a network room, which"},
    {HelpLine::Note,    "pages opponent boards instead of showing them at once."},
    {HelpLine::Blank,   ""},

    {HelpLine::Heading, "ATTACK BUBBLES"},
    {HelpLine::Body,    "Pop more than two bubbles at once and the surplus is"},
    {HelpLine::Body,    "sent at your opponents as attack bubbles (malus)."},
    {HelpLine::Body,    "The count is: bubbles popped, plus any left hanging"},
    {HelpLine::Body,    "that then fall, minus two."},
    {HelpLine::Blank,   ""},
    {HelpLine::Bullet,  "ON  -  everything you earn is sent straight out,"},
    {HelpLine::Body,    "    and everything sent at you lands."},
    {HelpLine::Bullet,  "OFF  -  nobody attacks anybody. A pure race."},
    {HelpLine::Bullet,  "Blockable  -  what you earn first pays down what is"},
    {HelpLine::Body,    "    still queued against you, and only the surplus"},
    {HelpLine::Body,    "    is sent on."},
    {HelpLine::Blank,   ""},
    {HelpLine::Note,    "Blocking never goes negative. Blocking more than you"},
    {HelpLine::Note,    "owe just empties your queue and sends nothing - it"},
    {HelpLine::Note,    "does not bank credit against the next wave. Only"},
    {HelpLine::Note,    "bubbles still queued can be blocked; once one has"},
    {HelpLine::Note,    "started falling it is committed and will land. A"},
    {HelpLine::Note,    "\"Blocked -N\" toast shows when a block happens."},
    {HelpLine::Blank,   ""},

    {HelpLine::Heading, "WHO YOUR ATTACK HITS"},
    {HelpLine::Body,    "Every living opponent gets the FULL attack - it is not"},
    {HelpLine::Body,    "divided between them. Pop enough for six attack bubbles"},
    {HelpLine::Body,    "in a four-player game and all three opponents receive"},
    {HelpLine::Body,    "six each, not two each."},
    {HelpLine::Blank,   ""},
    {HelpLine::Note,    "This is the original game's local rule, and it makes a"},
    {HelpLine::Note,    "big local match far more violent than an online one of"},
    {HelpLine::Note,    "the same size, where the same pop would be split. The"},
    {HelpLine::Note,    "online-only rules - splitting, solo targetting, and the"},
    {HelpLine::Note,    "single-target switch once six or more players are alive"},
    {HelpLine::Note,    "- do not apply here at all. Five seats is the cap, so"},
    {HelpLine::Note,    "the six-or-more rule can never come up locally."},
    {HelpLine::Blank,   ""},

    {HelpLine::Heading, "HOW ATTACKS LAND"},
    {HelpLine::Body,    "Attack bubbles wait briefly, then drop into your board"},
    {HelpLine::Body,    "in a wave. Each one picks a column at random and"},
    {HelpLine::Body,    "parks one row under whatever it meets there, so a"},
    {HelpLine::Body,    "single wave can hit the same column more than once."},
    {HelpLine::Body,    "At most seven fall at a time; the rest keep queuing."},
    {HelpLine::Blank,   ""},

    {HelpLine::Heading, "MODE"},
    {HelpLine::Bullet,  "Classic  -  survive; you lose when your board reaches"},
    {HelpLine::Body,    "    the bottom line. Last one standing wins."},
    {HelpLine::Bullet,  "Clear  -  race to empty your board. Forces row collapse"},
    {HelpLine::Body,    "    and attack bubbles off while it is on, and restores"},
    {HelpLine::Body,    "    your old choices when switched off."},
    {HelpLine::Blank,   ""},

    {HelpLine::Heading, "TEAM MODE"},
    {HelpLine::Body,    "Splits the odd seats against the even ones, so the"},
    {HelpLine::Body,    "pairing depends on how many are playing - the row"},
    {HelpLine::Body,    "spells out the actual split rather than naming a fixed"},
    {HelpLine::Body,    "one. Attacks skip your own team."},
    {HelpLine::Blank,   ""},

    {HelpLine::Heading, "CHAIN REACTION"},
    {HelpLine::Body,    "When on, bubbles knocked loose by a pop travel to an"},
    {HelpLine::Body,    "opponent's board on the way down instead of simply"},
    {HelpLine::Body,    "falling away. Off is the quieter, more predictable"},
    {HelpLine::Body,    "game; it does not change the attack count itself."},
    {HelpLine::Blank,   ""},

    {HelpLine::Heading, "ROW COLLAPSE"},
    {HelpLine::Body,    "On, the board drops a new row when you go too long"},
    {HelpLine::Body,    "without a match. Off removes that pressure entirely,"},
    {HelpLine::Body,    "which makes for much longer rounds."},
    {HelpLine::Blank,   ""},

    {HelpLine::Heading, "VICTORIES LIMIT"},
    {HelpLine::Body,    "How many rounds someone must win to take the match."},
    {HelpLine::Body,    "\"none\" plays on until you quit."},
    {HelpLine::Blank,   ""},

    {HelpLine::Heading, "BOTS"},
    {HelpLine::Body,    "Bots fill the last seats, capped at one fewer than the"},
    {HelpLine::Body,    "player count. Bot skill - Low, Med or High - sets how"},
    {HelpLine::Body,    "hard they all try; a stronger bot also reacts faster,"},
    {HelpLine::Body,    "not just more accurately."},
    {HelpLine::Blank,   ""},
    {HelpLine::Note,    "Unlike a network room, every local bot shares the one"},
    {HelpLine::Note,    "skill setting - there is no mixed field here."},
    {HelpLine::Blank,   ""},

    {HelpLine::Heading, "PER-PLAYER"},
    {HelpLine::Bullet,  "Aim guide  -  draws the bounce line that seat's shot"},
    {HelpLine::Body,    "    will follow. Set it per player, so a newcomer can"},
    {HelpLine::Body,    "    have it while everyone else plays without."},
    {HelpLine::Bullet,  "Max colors  -  5 to 8 for that seat. Fewer colors"},
    {HelpLine::Body,    "    means easier matches and faster pops, which is the"},
    {HelpLine::Body,    "    other way to even out a mismatched game."},
};

struct HelpPage { const HelpLine* lines; int count; const char* title; };

HelpPage PageFor(HelpTopic topic) {
    if (topic == HelpTopic::LocalMultiplayer)
        return {kLocalHelp, (int)(sizeof(kLocalHelp) / sizeof(kLocalHelp[0])),
                "LOCAL MULTIPLAYER  \xe2\x80\x94  SETTINGS GUIDE"};
    return {kOnlineHelp, (int)(sizeof(kOnlineHelp) / sizeof(kOnlineHelp[0])),
            "ONLINE MULTIPLAYER  \xe2\x80\x94  SETTINGS GUIDE"};
}

} // namespace

void MainMenu::HelpPanelRender() {
    if (!showingHelpPanel) return;

    SDL_Renderer* rend = const_cast<SDL_Renderer*>(renderer);
    menulist::DrawWorldMapBackdrop(rend, netGameBackground);

    auto tap = [&](int index, const SDL_Rect& rect, int subIndex, bool splitAdjust,
                   SDL_Keycode key) { AddPanelTapRow(index, rect, subIndex, splitAdjust, key); };

    // The page owns the tap rows while it is open, so a tap cannot reach the
    // room list underneath it. BeginPanelTapRows resets what the room
    // published earlier this frame -- HelpPanelRender runs after NetPanelRender
    // in MainMenu::Render for exactly that reason.
    BeginPanelTapRows(&helpMenuIndex);

    // "Close" is the header action, matching every other full-screen panel's
    // right-hand header button.
    const HelpPage page = PageFor((HelpTopic)helpTopic);
    menulist::DrawHeaderBar(rend, panelText, menulist::kHeaderBar, page.title,
                            "Close", helpMenuIndex == kHelpRowClose, kHelpRowClose,
                            tap, menulist::kMapFillAlpha);

    // One wide body column rather than the list+sidebar split every settings
    // screen uses: this is prose, and 404px of it wraps to about six words a
    // line, which reads badly over this many paragraphs.
    const SDL_Rect body = {10, 44, 620, 400};
    SDL_Color fill = menulist::kListFill;
    fill.a = menulist::kMapFillAlpha;
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(rend, fill.r, fill.g, fill.b, fill.a);
    { SDL_FRect fr = ToFRect(body); SDL_RenderFillRect(rend, &fr); }
    SDL_SetRenderDrawColor(rend, menulist::kEdge.r, menulist::kEdge.g,
                           menulist::kEdge.b, menulist::kEdge.a);
    { SDL_FRect fr = ToFRect(body); SDL_RenderRect(rend, &fr); }

    const int total = page.count;
    const int lineH = 16;
    const int visible = (body.h - 16) / lineH;
    const int maxScroll = std::max(0, total - visible);
    helpScroll = std::clamp(helpScroll, 0, maxScroll);

    SDL_SetRenderClipRect(rend, &body);
    int y = body.y + 8;
    for (int i = helpScroll; i < total && i < helpScroll + visible; i++) {
        const HelpLine& l = page.lines[i];
        switch (l.kind) {
            case HelpLine::Blank:
                break;
            case HelpLine::Heading:
                panelText.UpdateStyle(15, TTF_STYLE_BOLD);
                panelText.UpdateColor(menulist::kGold, menulist::kTextShadow);
                panelText.UpdateText(rend, l.text, 0);
                panelText.UpdatePosition({body.x + 14, y});
                { SDL_FRect fr = ToFRect(*panelText.Coords());
                  SDL_RenderTexture(rend, panelText.Texture(), nullptr, &fr); }
                SDL_SetRenderDrawColor(rend, menulist::kEdge.r, menulist::kEdge.g,
                                       menulist::kEdge.b, 110);
                SDL_RenderLine(rend, (float)(body.x + 14), (float)(y + lineH - 2),
                               (float)(body.x + body.w - 14), (float)(y + lineH - 2));
                break;
            case HelpLine::Bullet:
                panelText.UpdateStyle(14, TTF_STYLE_NORMAL);
                panelText.UpdateColor(menulist::kText, menulist::kTextShadow);
                panelText.UpdateText(rend, l.text, 0);
                panelText.UpdatePosition({body.x + 24, y});
                { SDL_FRect fr = ToFRect(*panelText.Coords());
                  SDL_RenderTexture(rend, panelText.Texture(), nullptr, &fr); }
                break;
            case HelpLine::Note:
                panelText.UpdateStyle(13, TTF_STYLE_ITALIC);
                panelText.UpdateColor(menulist::kMuted, menulist::kTextShadow);
                panelText.UpdateText(rend, l.text, 0);
                panelText.UpdatePosition({body.x + 24, y});
                { SDL_FRect fr = ToFRect(*panelText.Coords());
                  SDL_RenderTexture(rend, panelText.Texture(), nullptr, &fr); }
                break;
            case HelpLine::Body:
            default:
                panelText.UpdateStyle(14, TTF_STYLE_NORMAL);
                panelText.UpdateColor(menulist::kText, menulist::kTextShadow);
                panelText.UpdateText(rend, l.text, 0);
                panelText.UpdatePosition({body.x + 14, y});
                { SDL_FRect fr = ToFRect(*panelText.Coords());
                  SDL_RenderTexture(rend, panelText.Texture(), nullptr, &fr); }
                break;
        }
        y += lineH;
    }
    SDL_SetRenderClipRect(rend, nullptr);

    // Scrollbar, drawn only when the page actually overflows.
    if (maxScroll > 0) {
        const int trackH = body.h - 16;
        const int thumbH = std::max(24, trackH * visible / total);
        const int thumbY = body.y + 8 + (trackH - thumbH) * helpScroll / maxScroll;
        SDL_SetRenderDrawColor(rend, menulist::kEdge.r, menulist::kEdge.g,
                               menulist::kEdge.b, 90);
        { SDL_Rect t = {body.x + body.w - 8, body.y + 8, 3, trackH};
          SDL_FRect fr = ToFRect(t); SDL_RenderFillRect(rend, &fr); }
        SDL_SetRenderDrawColor(rend, menulist::kGold.r, menulist::kGold.g,
                               menulist::kGold.b, 220);
        { SDL_Rect t = {body.x + body.w - 9, thumbY, 5, thumbH};
          SDL_FRect fr = ToFRect(t); SDL_RenderFillRect(rend, &fr); }
    }

    // Two tap bands over the body itself, so the page can be paged through by
    // touch without a scrollbar to drag: upper half scrolls back, lower half
    // forward. Registered after the header action so its own band still wins.
    tap(kHelpRowScrollUp,   {body.x, body.y, body.w - 12, body.h / 2}, -1, false, SDLK_UP);
    tap(kHelpRowScrollDown, {body.x, body.y + body.h / 2, body.w - 12, body.h / 2},
        -1, false, SDLK_DOWN);

    menulist::DrawFooterHint(rend, panelText,
                             "Up/Down scroll   -   ESC or Close to go back");
}
