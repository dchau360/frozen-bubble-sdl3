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

#include "mainmenu.h"
#include "netteams.h"
#include "audiomixer.h"
#include "frozenbubble.h"
#include "localmultiplayer_settings.h"
#include "netbot.h"
#include "transitionmanager.h"
#include "networkclient.h"
#include "platform.h"

#include <SDL3_image/SDL_image.h>
#include <cstring>
#include <cmath>
#include <errno.h>
#include <thread>
#include <mutex>
#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#endif
#include "socket_compat.h"
#ifndef _WIN32
#ifndef __WASM_PORT__
#include <netdb.h>
#endif
#endif
#ifdef __WASM_PORT__
#include <emscripten.h>
#include <stdlib.h>
#endif

#include "mainmenu_internal.h"
#include "textinput.h"

void MainMenu::HandleInput(SDL_Event *e){
    switch(e->type) {
        case SDL_EVENT_TEXT_INPUT:
            MenuTextInputEvent(e);
            break;
        case SDL_EVENT_KEY_DOWN:
            // [A] opens the full-screen team picker (mainmenu_teampanel.cpp)
            // from anywhere in a network room. It used to enter a
            // cycle-in-place mode over the >5-cap roster instead, which could
            // only step one team at a time through a row a couple of
            // millimetres tall and never showed what the teams were; the page
            // draws every team as its own button on every player's row.
            // Offered for both room sizes now -- the <=5-cap grid's own Team
            // cells are no easier to hit, and this key did nothing there
            // before.
            // NOTE: 'T' was tried first but collides with the existing
            // chat-open hotkey (case SDLK_T below) -- C/R/J/T/U/S/P/N/M are
            // all already claimed single-letter hotkeys on this screen.
            if (!showingTeamsPanel && showingNetPanel && networkInLobby && networkInputMode == 0 &&
                selectedActionIndex != 0 && e->key.key == SDLK_A) {
                NetworkClient* netClientT = NetworkClient::Instance();
                GameRoom* curGameT = netClientT->GetCurrentGame();
                if (curGameT) {
                    OpenTeamsPanel();
                    return;
                }
            }

            // The settings guide is modal over whatever opened it, so it takes
            // keys before every other panel -- otherwise Up/Down would scroll
            // the page and move the room's selection underneath it at once.
            if (HelpPanelKey(e)) break;

            // The team picker is modal over the room in the same way, and for
            // the same reason: its own Up/Down walks players and its
            // Left/Right changes a team, both of which the room underneath
            // would otherwise also act on. Checked after the guide so the
            // guide still wins if it is open over this page.
            if (TeamsPanelKey(e)) break;

            // F1 opens the guide from either screen that has a HELP box, and
            // is also the key those boxes' tap targets send (see
            // kRoomHelpTapIndex / kLocalMPHelpTapIndex) -- so touch and
            // keyboard reach the same place through one path. Which page opens
            // is decided by which screen asked, not by the box that was
            // tapped, so the two cannot disagree.
            if (!showingHelpPanel && e->key.key == SDLK_F1) {
                const bool inRoom = showingNetPanel && networkInLobby &&
                                    networkInputMode == 0 &&
                                    NetworkClient::Instance()->GetCurrentGame() != nullptr;
                const bool inLocalMP = showingLocalMPPanel && !runDelay;
                if (inRoom || inLocalMP) {
                    showingHelpPanel = true;
                    helpTopic = (int)(inRoom ? HelpTopic::OnlineRoom
                                             : HelpTopic::LocalMultiplayer);
                    helpScroll = 0;
                    helpMenuIndex = kHelpRowClose;
                    PlayMenuSFX("menu_selected");
                    break;
                }
            }

            if (MenuEditingKey(e)) break;

            if (KeysPanelKey(e)) break;

            if (LobbyChatTypingKey(e)) break;

            if (LocalMPPanelKey(e)) break;

            switch(e->key.key) {
                case SDLK_UP:
                    MenuUpKey();
                    break;
                case SDLK_DOWN:
                    MenuDownKey();
                    break;
                case SDLK_LEFT:
                case SDLK_RIGHT:
                    MenuLeftRightKey(e);
                    break;
                case SDLK_RETURN:
                    MenuReturnKey();
                    break;
                case SDLK_C:
                    if (showingNetPanel && networkInLobby && networkInputMode == 0) {
                        NetworkClient* netClient = NetworkClient::Instance();
                        // Accept CONNECTED or IN_LOBBY — after returning from a game state is IN_LOBBY
                        if (netClient->GetState() == CONNECTED || netClient->GetState() == IN_LOBBY) {
                            if (!netClient->GetCurrentGame()) {
                                // Open create game confirmation screen
                                networkInputMode = 6;
                                AudioMixer::Instance()->PlaySFX("menu_selected");
                            }
                        }
                    }
                    break;
                case SDLK_R:
                    // Both refreshes used to run DiscoverLANServers/FetchPublicServers
                    // plus a per-server MeasureLatency synchronously right here --
                    // up to 1s+2s*N (LAN) or 16s+2s*N (public) of frozen UI on every
                    // R press. The initial fetch (ShowPanel/case 3 and case 5) was
                    // already threaded; this was the one inconsistent path. See the
                    // async networking handoff doc, stage 1d.
                    if (showingNetPanel && !networkInLobby && networkInputMode == 7) {
                        connectErrorMsg.clear();
                        lanMenuIndex = 0;
#ifdef __WASM_PORT__
                        discoveredServers = NetworkClient::DiscoverLANServers();
#else
                        discoveredServers.clear();
                        StartLanFetch();
#endif
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    }
                    if (showingNetPanel && !networkInLobby && networkInputMode == 10) {
                        connectErrorMsg.clear();
                        netMenuIndex = 0;
#ifdef __WASM_PORT__
                        publicServers = NetworkClient::FetchPublicServers();
#else
                        publicServers.clear();
                        StartPublicServerFetch();
#endif
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    }
                    break;
                case SDLK_J:
                    if (showingNetPanel && networkInLobby) {
                        NetworkClient* netClient = NetworkClient::Instance();
                        if (netClient->GetState() == CONNECTED) {
                            std::vector<GameRoom> games = netClient->GetGameList();
                            // If a game is selected, join it directly
                            if (selectedGameIndex >= 0 && selectedGameIndex < (int)games.size()) {
                                if (netClient->JoinGame(games[selectedGameIndex].creator.c_str())) {
                                    SDL_Log("Joining game created by %s", games[selectedGameIndex].creator.c_str());
                                    AudioMixer::Instance()->PlaySFX("menu_selected");
                                }
                            } else {
                                // No game selected, enter manual join mode
                                networkInputMode = 3;
                                networkJoinCreator[0] = '\0';
                                SDL_Log("Enter creator name to join their game");
                            }
                        }
                    }
                    break;
                case SDLK_T:
                    if (showingNetPanel && networkInLobby && networkInputMode == 0) {
                        // Enter chat mode
                        networkInputMode = 4;
                        networkChatInput[0] = '\0';
                        SDL_StopTextInput(SDL_GetKeyboardFocus());
                        SDL_StartTextInput(SDL_GetKeyboardFocus());
                        SetTextInputAreaLogical(const_cast<SDL_Renderer*>(renderer), {160, 152, 320, 20});
                        AudioMixer::Instance()->PlaySFX("menu_selected");
                    }
                    break;
                case SDLK_U:
                    if (showingNetPanel && networkInLobby && networkInputMode == 0) {
                        // Enter username change mode
                        networkInputMode = 5;
                        networkUsername[0] = '\0';
                        SDL_StopTextInput(SDL_GetKeyboardFocus());
                        SDL_StartTextInput(SDL_GetKeyboardFocus());
                        SetTextInputAreaLogical(const_cast<SDL_Renderer*>(renderer), {160, 152, 320, 20});
                        AudioMixer::Instance()->PlaySFX("menu_selected");
                    }
                    break;
                case SDLK_S:
                    if (showingNetPanel && networkInLobby) {
                        NetworkClient* netClient = NetworkClient::Instance();
                        GameRoom* currentGame = netClient->GetCurrentGame();
                        // Only allow host to start game with 'S' key
                        if (currentGame && netClient->GetState() == IN_LOBBY) {
                            bool isHost = currentGame->creator == netClient->GetPlayerNick();
                            if (isHost && currentGame->players.size() > 1) {
                                netClient->StartGame();
                                netClient->AddStatusMessage("Starting game...");
                            }
                        }
                    }
                    break;
                case SDLK_P:
                    if (showingNetPanel && networkInLobby) {
                        NetworkClient* netClient = NetworkClient::Instance();
                        if (netClient->GetState() == IN_LOBBY) {
                            showingTeamsPanel = false;
                            netStartRequested = false;  // don't carry a pending Start into the next room
                            DropLobbyBots();  // our bots leave with us
                            netClient->PartGame();
                            netClient->RequestList();  // Immediate list after parting
                            lastListRequest = SDL_GetTicks();
                        }
                    }
                    break;
                case SDLK_N:
                    if(SDL_GetKeyboardState(NULL)[SDL_SCANCODE_LCTRL]) RefreshCandy();
                    break;
                case SDLK_AC_BACK:
                case SDLK_ESCAPE:
                    MenuEscapeKey();
                    break;
                case SDLK_F11: // mute / unmute audio
                    // Resumes the paused track rather than restarting "intro".
                    if(AudioMixer::Instance()->IsHalted() == true)
                        AudioMixer::Instance()->MuteAll(true);
                    else AudioMixer::Instance()->MuteAll();
                    break;
            }
            break;
    }
}

void MainMenu::MenuTextInputEvent(SDL_Event *e) {
            // Handle virtual keyboard character input for host field (mode 8, only when actively editing)
            if (showingNetPanel && !networkInLobby && networkInputMode == 8 && networkFieldEditing) {
                size_t len = strlen(networkHost);
                for (const char* p = e->text.text; *p; p++) {
                    char c = *p;
                    if (c == '\b') {
                        // Android virtual keyboard sends backspace as '\b' in TEXTINPUT
                        if (len > 0) { networkHost[--len] = '\0'; }
                    } else if (len < 255 && ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
                        (c >= 'A' && c <= 'Z') || c == '.' || c == '-' || c == ':')) {
                        networkHost[len++] = c;
                        networkHost[len] = '\0';
                    }
                }
            }
            // Handle virtual keyboard character input for nickname field (mode 11)
            if (networkInputMode == 11) {
                AppendUtf8Input(networkPreNick, e->text.text, 15);
            }
            // Handle virtual keyboard character input for the post-stats-opt-in
            // nickname prompt -- opened from the 1-player submenu now, not
            // this panel (see KeysPanelKey), so not gated on showingKeysPanel.
            if (showingStatsNicknamePrompt) {
                AppendUtf8Input(statsUploadNickname, e->text.text, 15);
            }
            // Handle virtual keyboard character input for chat (mode 4)
            if (showingNetPanel && networkInLobby && networkInputMode == 4) {
                AppendUtf8Input(networkChatInput, e->text.text);
            }
            // Handle virtual keyboard character input for username (mode 5)
            if (showingNetPanel && networkInLobby && networkInputMode == 5) {
                AppendUtf8Input(networkUsername, e->text.text, 31);
            }
}

bool MainMenu::MenuEditingKey(SDL_Event *e) {
            // Handle backspace/delete in text fields before the repeat filter,
            // because Android's IME may send backspace with repeat=1 on a single press.
            if (e->key.key == SDLK_BACKSPACE || e->key.key == SDLK_DELETE) {
                if (showingNetPanel && !networkInLobby && networkInputMode == 8 && networkFieldEditing) {
                    size_t len = strlen(networkHost);
                    if (len > 0) networkHost[len - 1] = '\0';
                    return true;
                }
                if (showingNetPanel && !networkInLobby && networkInputMode == 11) {
                    BackspaceUtf8(networkPreNick);
                    return true;
                }
                if (showingStatsNicknamePrompt) {
                    BackspaceUtf8(statsUploadNickname);
                    return true;
                }
            }
            if(e->key.repeat) return true;

            // Handle network panel text input
            if (showingNetPanel && networkInLobby && networkInputMode == 3) {
                // Join creator name input
                if ((e->key.key >= SDLK_A && e->key.key <= SDLK_Z) ||
                    (e->key.key >= SDLK_0 && e->key.key <= SDLK_9)) {
                    size_t len = strlen(networkJoinCreator);
                    if (len < 31) {
                        if (e->key.key >= SDLK_A && e->key.key <= SDLK_Z) {
                            networkJoinCreator[len] = 'a' + (e->key.key - SDLK_A);
                        } else {
                            networkJoinCreator[len] = '0' + (e->key.key - SDLK_0);
                        }
                        networkJoinCreator[len + 1] = '\0';
                    }
                    return true;
                } else if (e->key.key == SDLK_BACKSPACE) {
                    size_t len = strlen(networkJoinCreator);
                    if (len > 0) networkJoinCreator[len - 1] = '\0';
                    return true;
                }
            } else if (showingNetPanel && networkInLobby && networkInputMode == 4) {
                // Chat input - characters handled by SDL_EVENT_TEXT_INPUT
                if (e->key.key == SDLK_BACKSPACE) {
                    BackspaceUtf8(networkChatInput);
                    return true;
                }
#ifdef __WASM_PORT__
                // See the matching fix in BubbleGame::HandleInput's chattingMode
                // block: Emscripten's SDL3 port never emits a space through
                // SDL_EVENT_TEXT_INPUT, so it has to be appended from the keydown
                // instead. Native builds already get a real space through TEXT_INPUT.
                else if (e->key.key == SDLK_SPACE) {
                    size_t len = strlen(networkChatInput);
                    if (len + 1 < sizeof(networkChatInput)) {
                        networkChatInput[len] = ' ';
                        networkChatInput[len + 1] = '\0';
                    }
                    return true;
                }
#endif
            } else if (showingNetPanel && networkInLobby && networkInputMode == 5) {
                // Username input - characters handled by SDL_EVENT_TEXT_INPUT
                if (e->key.key == SDLK_BACKSPACE) {
                    BackspaceUtf8(networkUsername);
                    return true;
                }
#ifdef __WASM_PORT__
                else if (e->key.key == SDLK_SPACE) {
                    size_t len = strlen(networkUsername);
                    if (len + 1 < sizeof(networkUsername)) {
                        networkUsername[len] = ' ';
                        networkUsername[len + 1] = '\0';
                    }
                    return true;
                }
#endif
            } else if (showingNetPanel && !networkInLobby && networkInputMode == 11) {
                // Pre-lobby nickname input - characters handled by SDL_EVENT_TEXT_INPUT
                if (e->key.key == SDLK_BACKSPACE) {
                    BackspaceUtf8(networkPreNick);
                    return true;
                }
#ifdef __WASM_PORT__
                else if (e->key.key == SDLK_SPACE) {
                    size_t len = strlen(networkPreNick);
                    if (len < 15) {  // matches MenuTextInputEvent's cap for this field
                        networkPreNick[len] = ' ';
                        networkPreNick[len + 1] = '\0';
                    }
                    return true;
                }
#endif
            }

            // Handle backspace in level panel
            if (showingLevelPanel && !runDelay) {
                if (e->key.key == SDLK_BACKSPACE) {
                    if (!levelInput.empty()) levelInput.pop_back();
                    return true;
                }
            }

            // Handle backspace in chat input when Chat is selected in lobby
            if (showingNetPanel && networkInLobby && networkInputMode == 0 && selectedActionIndex == 0) {
                if (e->key.key == SDLK_BACKSPACE) {
                    BackspaceUtf8(networkChatInput);
                    return true;
                }
            }

            // Digit input for level selection panel
            if (showingLevelPanel && !runDelay) {
                if (e->key.key >= SDLK_0 && e->key.key <= SDLK_9) {
                    if (levelInput.size() < 3) {
                        levelInput += (char)('0' + (e->key.key - SDLK_0));
                    }
                    return true;
                }
            }

            if (showingNetPanel && !networkInLobby && (networkInputMode == 8 || networkInputMode == 9)) {
                // Manual entry form: host (index 0), port (index 1), connect (index 2)
                if (networkFieldEditing) {
                    // Keyboard is open — handle field-specific input
                    if (networkManualFieldIndex == 0) {
                        // Host field editing
                        connectErrorMsg.clear();
                        size_t len = strlen(networkHost);
                        if (e->key.key == SDLK_BACKSPACE || e->key.key == SDLK_DELETE) {
                            if (len > 0) networkHost[len - 1] = '\0';
                            return true;
                        }
                    } else if (networkManualFieldIndex == 1) {
                        // Port field editing
                        if (e->key.key >= SDLK_0 && e->key.key <= SDLK_9) {
                            if (networkPort < 6553) networkPort = networkPort * 10 + (e->key.key - SDLK_0);
                            return true;
                        } else if (e->key.key == SDLK_BACKSPACE) {
                            networkPort /= 10;
                            return true;
                        }
                    }
                } else {
                    // Not editing: UP/DOWN cycles through host, port, connect
                    if (e->key.key == SDLK_DOWN) {
                        networkManualFieldIndex = (networkManualFieldIndex + 1) % 3;
                        networkInputMode = (networkManualFieldIndex == 1) ? 9 : 8;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                        return true;
                    } else if (e->key.key == SDLK_UP) {
                        networkManualFieldIndex = (networkManualFieldIndex + 2) % 3;
                        networkInputMode = (networkManualFieldIndex == 1) ? 9 : 8;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                        return true;
                    }
                }
            }

            if (awaitKp && (showingOptPanel || showingNetSetupPanel) && e->key.key != SDLK_ESCAPE) {
                AudioMixer::Instance()->PlaySFX("typewriter");
                lastOptInput = e->key.key;
                awaitKp = false;
                return true;
            }
    return false;
}

void MainMenu::BeginPanelTapRows(int* selection, int* subSelection) {
    panelTapRows.clear();
    panelTapSelection = selection;
    panelTapSubSelection = subSelection;
}

void MainMenu::AddPanelTapRow(int index, const SDL_Rect& rect, int subIndex,
                              bool splitAdjust, SDL_Keycode activateKey) {
    if (rect.w <= 0 || rect.h <= 0) return;  // row not drawn this frame
    panelTapRows.push_back({rect, index, subIndex, splitAdjust, activateKey});
}

bool MainMenu::HandlePanelTap(float lx, float ly, float verticalDrift) {
    // "Tap here to cancel" on the connecting indicator. Checked before the
    // server list's own rows because it is drawn over them, and because a tap
    // landing on it means the player wants out of the connect -- not to start
    // a second one against whichever row happens to sit underneath. The rect
    // is zeroed whenever the indicator is not showing, so this is inert the
    // rest of the time (async networking handoff, stage 2e).
    if (cancelConnectTapRect.w > 0 &&
        lx >= cancelConnectTapRect.x && lx < cancelConnectTapRect.x + cancelConnectTapRect.w &&
        ly >= cancelConnectTapRect.y && ly < cancelConnectTapRect.y + cancelConnectTapRect.h) {
        AudioMixer::Instance()->PlaySFX("cancel");
        CancelPendingConnect();
        return true;
    }
    // The team picker is drawn over the room whose rows are still registered
    // underneath it, and hit-tests its own swatch rects instead -- first, and
    // consuming every tap, for the same reason as the popups below.
    if (showingTeamsPanel) return HandleTeamsPanelTap(lx, ly);
    // The "enable Arcade Mode?" popup (opened from the 1-player submenu, see
    // SPPanelRender/press()) -- same reasoning as showingStatsUploadConfirm
    // below: modal, sits on top of whichever panel is underneath, and every
    // tap is consumed while it's showing, hit or miss.
    if (showingArcadeModeConfirm) {
        auto hit = [&](const SDL_Rect& r) {
            return lx >= r.x && lx < r.x + r.w && ly >= r.y && ly < r.y + r.h;
        };
        SDL_Keycode key = SDLK_UNKNOWN;
        // Force focus to match whichever button was actually tapped before
        // pushing the synthetic RETURN -- KeysPanelKey's RETURN handling
        // below now reads confirmDialogFocusNo to decide Turn On vs Cancel,
        // and a tap must win over whatever a keyboard/gamepad left it at.
        if (hit(arcadeConfirmYesRect)) { confirmDialogFocusNo = false; key = SDLK_RETURN; }
        else if (hit(arcadeConfirmNoRect)) key = SDLK_ESCAPE;
        if (key != SDLK_UNKNOWN) {
            SDL_Event ev = {};
            ev.type = SDL_EVENT_KEY_DOWN;
            ev.key.key = key;
            SDL_PushEvent(&ev);
        }
        return true;
    }
    // The "upload highscore stats?" popup (opened from the 1-player submenu,
    // see SPPanelRender/press()) is modal and sits on top of whichever panel
    // is underneath, but panelTapRows still holds that panel's rows -- checked
    // here, first, so a tap never falls through to whichever hidden row
    // happens to occupy this screen position. Every tap is consumed while
    // this is showing, hit or miss, since letting a miss fall through would
    // let it silently move the row/button selection underneath (previously
    // the only way to react to this popup at all was a physical ENTER/ESC
    // keypress -- there was no touch equivalent). Not gated on which panel is
    // open: only one of them ever sets these flags at a time.
    if (showingStatsNicknamePrompt) {
        auto hit = [&](const SDL_Rect& r) {
            return lx >= r.x && lx < r.x + r.w && ly >= r.y && ly < r.y + r.h;
        };
        SDL_Keycode key = SDLK_UNKNOWN;
        if (hit(statsNicknameSaveRect)) { confirmDialogFocusNo = false; key = SDLK_RETURN; }
        else if (hit(statsNicknameSkipRect)) key = SDLK_ESCAPE;
        if (key != SDLK_UNKNOWN) {
            SDL_Event ev = {};
            ev.type = SDL_EVENT_KEY_DOWN;
            ev.key.key = key;
            SDL_PushEvent(&ev);
        }
        return true;
    }
    if (showingStatsUploadConfirm) {
        auto hit = [&](const SDL_Rect& r) {
            return lx >= r.x && lx < r.x + r.w && ly >= r.y && ly < r.y + r.h;
        };
        SDL_Keycode key = SDLK_UNKNOWN;
        if (hit(statsConfirmYesRect)) { confirmDialogFocusNo = false; key = SDLK_RETURN; }
        else if (hit(statsConfirmNoRect)) key = SDLK_ESCAPE;
        if (key != SDLK_UNKNOWN) {
            SDL_Event ev = {};
            ev.type = SDL_EVENT_KEY_DOWN;
            ev.key.key = key;
            SDL_PushEvent(&ev);
        }
        return true;
    }
    // The "Enable chain reaction? Y or N?" prompt (OptPanelRender /
    // NetSetupPanelRender -- random level, multiplayer training, and network
    // game all share it) is modal the same way, and registers no rows of its
    // own -- checked here for the same reason as showingStatsUploadConfirm
    // above, so a tap never falls through to whatever row list is stale
    // underneath. Previously any key but ESC answered this from a
    // keyboard/gamepad; there was no touch equivalent at all.
    if ((showingOptPanel || showingNetSetupPanel) && awaitKp) {
        auto hit = [&](const SDL_Rect& r) {
            return lx >= r.x && lx < r.x + r.w && ly >= r.y && ly < r.y + r.h;
        };
        if (hit(optYesRect)) {
            AudioMixer::Instance()->PlaySFX("typewriter");
            lastOptInput = SDLK_Y;
            awaitKp = false;
        } else if (hit(optNoRect)) {
            AudioMixer::Instance()->PlaySFX("typewriter");
            lastOptInput = SDLK_N;
            awaitKp = false;
        }
        return true;
    }
    // While the key panel is waiting for a key to bind, or a text field is being
    // edited, a tap is not a row press. Consuming it here would swallow the
    // gesture that gets the player back out of that state.
    if (showingKeysPanel && awaitKp) return false;
    if (networkFieldEditing) return false;
    if (panelTapSelection == nullptr) return false;

    for (const PanelTapRow& row : panelTapRows) {
        if (lx < row.rect.x || lx >= row.rect.x + row.rect.w) continue;
        if (ly < row.rect.y || ly >= row.rect.y + row.rect.h) continue;

        bool sameRow = (*panelTapSelection == row.index);
        // A grid cell is only "the same" when the column matches too, so moving
        // across a row of player columns stays a selection rather than
        // immediately changing the cell that happens to be under the finger.
        if (sameRow && row.subIndex >= 0 && panelTapSubSelection != nullptr)
            sameRow = (*panelTapSubSelection == row.subIndex);

        if (sameRow) {
            // A vertical swipe attempt that falls short of the Up/Down
            // threshold in FrozenBubble::HandleInput often still lands back
            // on the row it started from -- most rows are shorter than that
            // travel. Reaching this activation branch does not by itself
            // mean the player meant to activate the row rather than
            // navigate off it; only a near-stationary release does. Past
            // this budget the tap is still consumed (it landed on a real
            // row -- the caller must not fall back to tap-anywhere-confirms
            // either), just without mutating anything.
            constexpr float kActivateMaxVerticalDrift = 8.f;
            if (fabsf(verticalDrift) > kActivateMaxVerticalDrift) return true;
        }

        if (!sameRow) {
            // First tap only moves the highlight. Most rows change a setting on
            // activation, so a tap that both selected and activated would give
            // no chance to read a row before changing it -- and on a phone the
            // row under a thumb is easy to misjudge.
            *panelTapSelection = row.index;
            if (row.subIndex >= 0 && panelTapSubSelection != nullptr)
                *panelTapSubSelection = row.subIndex;
            resetAllArmed = false;
            PlayMenuSFX("menu_change");
            return true;
        }

        // The >5-cap compact roster's per-player rows (AddPanelTapRow call in
        // NetPanelLobbyActionsRender). Opens the full-screen team picker on
        // the tapped player's row, which is the one place teams are set from
        // now -- handled here rather than through the generic key push below
        // because which row was tapped has to travel with the tap, and an
        // injected keycode has nowhere to carry it.
        if (row.index >= kRoomRosterTapBase && row.index < kRoomRosterTapBase + kRoomRosterTapSlots) {
            OpenTeamsPanel(row.index - kRoomRosterTapBase);
            return true;
        }

        // Second tap on the already-highlighted row activates it. Pushed as a
        // key event rather than duplicating each panel's activation logic, so
        // every row keeps behaving exactly as it does from a keyboard.
        SDL_Event ev = {};
        ev.type = SDL_EVENT_KEY_DOWN;
        if (row.activateKey != 0) {
            ev.key.key = row.activateKey;
        } else if (row.splitAdjust) {
            // Stepped rows have no Return behaviour to borrow, so the half of
            // the row that was touched picks the direction instead.
            const float mid = row.rect.x + row.rect.w * 0.5f;
            ev.key.key = (lx < mid) ? SDLK_LEFT : SDLK_RIGHT;
        } else {
            ev.key.key = SDLK_RETURN;
        }
        SDL_PushEvent(&ev);
        return true;
    }

    // The tap hit no row. Returning false here means "this panel doesn't
    // hit-test taps", and the caller answers that by injecting RETURN
    // (tap anywhere confirms) -- which activates whatever row is currently
    // SELECTED. On a panel that does hit-test its rows that is badly wrong:
    // a near-miss doesn't do nothing, it re-fires the selected row.
    //
    // Reported live on the network game room's bot rows, where it is a trap
    // with no way out. Those rows are 18 logical units tall (the band under
    // "ESC Leave room" is all the space left on that panel), which on a
    // phone is a couple of millimetres, so with "Bots" selected almost every
    // tap aimed at another row misses everything, lands here, and injects
    // RETURN -- which cycles the bot count again. Trying to move somewhere
    // else just added another bot, over and over, with the selection never
    // leaving the row: "I can't navigate out of that location". The local
    // multiplayer panel's own Bots row does the same thing for the same
    // reason.
    //
    // So a miss is consumed once this panel registered any row at all.
    // Panels that register none still report false and keep the original
    // tap-anywhere-to-confirm behaviour, which is the only thing that makes
    // them tappable.
    return !panelTapRows.empty();
}

bool MainMenu::IsSteppedRowAt(float lx, float ly) const {
    for (const PanelTapRow& row : panelTapRows) {
        // menulist::List no longer registers a stepped row as one wide
        // splitAdjust rect (see List::End()) -- it registers two adjacent
        // rects sharing an index, each with activateKey pinned to the
        // direction its own half sends, so the boundary can sit at the
        // drawn "< value >" text instead of the row's raw geometric middle.
        // A caller asking "is this a stepped row" has to recognize both
        // shapes, or every List-rendered stepped row would silently stop
        // registering as one -- reopening the exact swipe-vs-tap collision
        // this function exists to prevent.
        const bool stepped = row.splitAdjust ||
                             row.activateKey == SDLK_LEFT || row.activateKey == SDLK_RIGHT;
        if (!stepped) continue;
        if (lx < row.rect.x || lx >= row.rect.x + row.rect.w) continue;
        if (ly < row.rect.y || ly >= row.rect.y + row.rect.h) continue;
        return true;
    }
    return false;
}

// Keys for the settings guide (mainmenu_help.cpp). Returns true when the key
// was consumed, so HandleInput stops before the room underneath sees it.
bool MainMenu::HelpPanelKey(SDL_Event *e) {
    if (!showingHelpPanel) return false;
    switch (e->key.key) {
        case SDLK_UP:       helpScroll -= 1;  break;
        case SDLK_DOWN:     helpScroll += 1;  break;
        case SDLK_PAGEUP:   helpScroll -= 12; break;
        case SDLK_PAGEDOWN: helpScroll += 12; break;
        case SDLK_HOME:     helpScroll = 0;   break;
        case SDLK_LEFT:
        case SDLK_RIGHT:
            // The page has one header action and nothing to step, so L/R
            // would otherwise fall through to the room and quietly change a
            // setting the player cannot see.
            break;
        case SDLK_RETURN:
        case SDLK_ESCAPE:
        case SDLK_AC_BACK:
        case SDLK_F1:
            showingHelpPanel = false;
            // The selection is still parked on whichever HELP box was tapped.
            // Neither fake index is a real row, so leave the player somewhere
            // Up/Down can work from rather than off the end of the list.
            if (selectedActionIndex == kRoomHelpTapIndex) selectedActionIndex = kRoomMalus;
            if (localMPMenuIndex == kLocalMPHelpTapIndex)
                localMPMenuIndex = LocalMPRowBotSkill(localMPGameMode);
            PlayMenuSFX("menu_change");
            return true;
        default:
            // Swallow everything else: the guide is modal.
            return true;
    }
    if (helpScroll < 0) helpScroll = 0;   // upper clamp is in HelpPanelRender,
                                          // which knows the page length
    PlayMenuSFX("menu_change");
    return true;
}

bool MainMenu::KeysPanelKey(SDL_Event *e) {
            // Opened from the 1-player submenu's "Arcade Mode" toggle (see
            // SPPanelRender/press() in mainmenu.cpp) but handled here rather
            // than a separate SPPanelKey of its own -- same reasoning as the
            // two stats-upload modals just below. LEFT/RIGHT/TAB move
            // keyboard focus between the two buttons (confirmDialogFocusNo,
            // shared by all three of this panel's two-button popups; see
            // mainmenu.h), ENTER activates whichever is focused, and ESC
            // still cancels outright as a shortcut regardless of focus.
            if (showingArcadeModeConfirm) {
                if (e->key.key == SDLK_LEFT || e->key.key == SDLK_RIGHT || e->key.key == SDLK_TAB) {
                    confirmDialogFocusNo = !confirmDialogFocusNo;
                    AudioMixer::Instance()->PlaySFX("menu_change");
                } else if (e->key.key == SDLK_RETURN && !confirmDialogFocusNo) {
                    GameSettings::Instance()->SetValue("Game:ArcadeMode", "");
                    showingArcadeModeConfirm = false;
                    confirmDialogFocusNo = false;
                    AudioMixer::Instance()->PlaySFX("typewriter");
                } else if (e->key.key == SDLK_RETURN || e->key.key == SDLK_ESCAPE || e->key.key == SDLK_AC_BACK) {
                    showingArcadeModeConfirm = false;
                    confirmDialogFocusNo = false;
                    AudioMixer::Instance()->PlaySFX("menu_change");
                }
                return true;
            }
            // These two modals are opened from the 1-player submenu's "Upload
            // highscore stats" toggle (see SPPanelRender/press() in
            // mainmenu.cpp) but handled here rather than duplicated in a
            // SPPanelKey of their own -- checked ahead of the showingKeysPanel
            // block below, and not gated on which panel is open, since only
            // one of them ever sets these flags at a time. Same
            // LEFT/RIGHT/TAB + ENTER/ESC focus scheme as showingArcadeModeConfirm
            // above.
            if (showingStatsNicknamePrompt) {
                // Typed characters/backspace arrive through
                // MenuTextInputEvent/MenuEditingKey; only focus-move and
                // Save/Skip matter here. Either finishes turning the
                // setting on: this is an offer to set a nickname, not a
                // second gate on it.
                if (e->key.key == SDLK_LEFT || e->key.key == SDLK_RIGHT || e->key.key == SDLK_TAB) {
                    confirmDialogFocusNo = !confirmDialogFocusNo;
                    AudioMixer::Instance()->PlaySFX("menu_change");
                } else if (e->key.key == SDLK_RETURN && !confirmDialogFocusNo) {
                    if (statsUploadNickname[0] != '\0') {
                        GameSettings* gs = GameSettings::Instance();
                        snprintf(gs->savedNickname, sizeof(gs->savedNickname), "%s", statsUploadNickname);
                        gs->SaveKeys();
                    }
                    showingStatsNicknamePrompt = false;
                    confirmDialogFocusNo = false;
                    SDL_StopTextInput(SDL_GetKeyboardFocus());
                    AudioMixer::Instance()->PlaySFX("typewriter");
                } else if (e->key.key == SDLK_RETURN || e->key.key == SDLK_ESCAPE || e->key.key == SDLK_AC_BACK) {
                    showingStatsNicknamePrompt = false;
                    confirmDialogFocusNo = false;
                    SDL_StopTextInput(SDL_GetKeyboardFocus());
                    AudioMixer::Instance()->PlaySFX("menu_change");
                }
                return true;
            }
            if (showingStatsUploadConfirm) {
                if (e->key.key == SDLK_LEFT || e->key.key == SDLK_RIGHT || e->key.key == SDLK_TAB) {
                    confirmDialogFocusNo = !confirmDialogFocusNo;
                    AudioMixer::Instance()->PlaySFX("menu_change");
                } else if (e->key.key == SDLK_RETURN && !confirmDialogFocusNo) {
                    GameSettings::Instance()->SetValue("Stats:UploadHighscore", "");
                    showingStatsUploadConfirm = false;
                    confirmDialogFocusNo = false;
                    AudioMixer::Instance()->PlaySFX("typewriter");
                    // Ask for a nickname right away, pre-filled with
                    // whatever is already saved (if anything) so this
                    // doubles as a chance to review/change it -- without
                    // this, a player with no nickname set would have
                    // their uploads silently read as "Anonymous", with
                    // the Net Game screen the only other place they could
                    // have discovered to set one.
                    showingStatsNicknamePrompt = true;
                    snprintf(statsUploadNickname, sizeof(statsUploadNickname), "%s",
                             GameSettings::Instance()->savedNickname);
                    SDL_StopTextInput(SDL_GetKeyboardFocus());
                    SDL_StartTextInput(SDL_GetKeyboardFocus());
                } else if (e->key.key == SDLK_RETURN || e->key.key == SDLK_ESCAPE || e->key.key == SDLK_AC_BACK) {
                    showingStatsUploadConfirm = false;
                    confirmDialogFocusNo = false;
                    AudioMixer::Instance()->PlaySFX("menu_change");
                }
                return true;
            }
            if (showingKeysPanel) {
                if (awaitKp && e->key.key != SDLK_ESCAPE) {
                    // Set the key for the current player/index
                    GameSettings* gs = GameSettings::Instance();
                    PlayerKeys* allKeys[5] = {
                        &gs->player1Keys, &gs->player2Keys, &gs->player3Keys,
                        &gs->player4Keys, &gs->player5Keys
                    };
                    PlayerKeys& keys = *allKeys[keyConfigPlayer - 1];
                    switch (keyConfigIndex) {
                        case 0: keys.left   = e->key.scancode; break;
                        case 1: keys.right  = e->key.scancode; break;
                        case 2: keys.fire   = e->key.scancode; break;
                        case 3: keys.center = e->key.scancode; break;
                    }
                    awaitKp = false;
                    AudioMixer::Instance()->PlaySFX("typewriter");
                    return true;
                } else if (!awaitKp) {
                    // Once ads are paid off, the two purchase rows collapse to
                    // a single thank-you line with nothing to activate. They
                    // keep their indices so the enum stays fixed, so navigation
                    // has to step over them or the highlight vanishes for two
                    // presses on a row that isn't drawn.
                    auto skippableRow = [](int row) {
#ifdef __ANDROID__
                        return (row == kKeyRowRemoveAdsMonth ||
                                row == kKeyRowRemoveAdsYear ||
                                row == kKeyRowRemoveAdsForever) && AdsRemoved();
#else
                        (void)row;
                        return false;
#endif
                    };
                    // UP/DOWN: navigate keys within current player
                    if (e->key.key == SDLK_UP) {
                        do {
                            keyConfigIndex = (keyConfigIndex == 0) ? kKeyRowLast : keyConfigIndex - 1;
                        } while (skippableRow(keyConfigIndex));
                        resetAllArmed = false;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                        return true;
                    } else if (e->key.key == SDLK_DOWN) {
                        do {
                            keyConfigIndex = (keyConfigIndex == kKeyRowLast) ? 0 : keyConfigIndex + 1;
                        } while (skippableRow(keyConfigIndex));
                        resetAllArmed = false;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                        return true;
                    } else if (e->key.key == SDLK_LEFT) {
                        if (keyConfigIndex == kKeyRowSpeed) {
                            // Decrease game speed
                            GameSettings* gs = GameSettings::Instance();
                            gs->speedMultiplier -= 0.1f;
                            if (gs->speedMultiplier < 1.0f) gs->speedMultiplier = 1.0f;
                            gs->SaveKeys();
                            AudioMixer::Instance()->PlaySFX("menu_change");
                            return true;
                        }
                        // Previous player
                        keyConfigPlayer = (keyConfigPlayer == 1) ? 4 : keyConfigPlayer - 1;
                        keyConfigIndex = 0;
                        resetAllArmed = false;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                        return true;
                    } else if (e->key.key == SDLK_RIGHT) {
                        if (keyConfigIndex == kKeyRowSpeed) {
                            // Increase game speed
                            GameSettings* gs = GameSettings::Instance();
                            gs->speedMultiplier += 0.1f;
                            if (gs->speedMultiplier > 5.0f) gs->speedMultiplier = 5.0f;
                            gs->SaveKeys();
                            AudioMixer::Instance()->PlaySFX("menu_change");
                            return true;
                        }
                        // Next player
                        keyConfigPlayer = (keyConfigPlayer == 4) ? 1 : keyConfigPlayer + 1;
                        keyConfigIndex = 0;
                        resetAllArmed = false;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                        return true;
                    } else if (e->key.key == SDLK_1 || e->key.key == SDLK_2 ||
                               e->key.key == SDLK_3 || e->key.key == SDLK_4) {
                        // Jump straight to a player, both a real keyboard shortcut
                        // and what a tap on that player's sidebar row sends (see
                        // KeysPanelRender's kKeyPlayerTapBase rows) -- the two-tap
                        // select-then-activate dance every other tap row uses, so
                        // touch keeps behaving exactly like a keypress.
                        if (e->key.key == SDLK_1) keyConfigPlayer = 1;
                        else if (e->key.key == SDLK_2) keyConfigPlayer = 2;
                        else if (e->key.key == SDLK_3) keyConfigPlayer = 3;
                        else keyConfigPlayer = 4;
                        keyConfigIndex = 0;
                        resetAllArmed = false;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                        return true;
                    } else if (e->key.key == SDLK_RETURN) {
                        if (keyConfigIndex == kKeyRowResetCtrl) {
                            // Reset current player to default controller bindings
                            GameSettings* gs = GameSettings::Instance();
                            PlayerKeys* allKeys[5] = {
                                &gs->player1Keys, &gs->player2Keys, &gs->player3Keys,
                                &gs->player4Keys, &gs->player5Keys
                            };
                            int slot = keyConfigPlayer - 1;
                            PlayerKeys& keys = *allKeys[slot];
                            keys.left   = (SDL_Scancode)(CTRL_SC_BASE + slot * 20 + SDL_GAMEPAD_BUTTON_DPAD_LEFT);
                            keys.right  = (SDL_Scancode)(CTRL_SC_BASE + slot * 20 + SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
                            keys.fire   = (SDL_Scancode)(CTRL_SC_BASE + slot * 20 + SDL_GAMEPAD_BUTTON_SOUTH);
                            keys.center = (SDL_Scancode)(CTRL_SC_BASE + slot * 20 + SDL_GAMEPAD_BUTTON_DPAD_DOWN);
                            gs->SaveKeys();
                            AudioMixer::Instance()->PlaySFX("typewriter");
                        } else if (keyConfigIndex == kKeyRowSpeed) {
                            // Game Speed row — adjusted via LEFT/RIGHT, ENTER does nothing
                        } else if (keyConfigIndex == kKeyRowSound) {
                            // Toggle sound on/off
                            GameSettings* gs = GameSettings::Instance();
                            bool nowOn = !gs->soundEnabled();
                            gs->setSoundEnabled(nowOn);
                            if (nowOn) {
                                AudioMixer::Instance()->MuteAll(true);
                                AudioMixer::Instance()->PlayMusic("intro");
                                AudioMixer::Instance()->PlaySFX("typewriter");
                            } else {
                                AudioMixer::Instance()->MuteAll(false);
                            }
                        } else if (keyConfigIndex == kKeyRowMouse) {
                            // Toggle mouse/touch aim
                            GameSettings* gs = GameSettings::Instance();
                            gs->mouseEnabled = !gs->mouseEnabled;
                            gs->SaveKeys();
                            AudioMixer::Instance()->PlaySFX("menu_change");
#ifndef __WASM_PORT__
                        } else if (keyConfigIndex == kKeyRowFullscreen) {
                            // Toggle fullscreen
                            GameSettings* gs = GameSettings::Instance();
                            gs->SetValue("GFX:Fullscreen", "");
                            SDL_SetWindowFullscreen(SDL_GetRenderWindow(const_cast<SDL_Renderer*>(renderer)), gs->fullscreenMode());
                            AudioMixer::Instance()->PlaySFX("menu_change");
#endif
#ifdef __ANDROID__
                        } else if (keyConfigIndex == kKeyRowRemoveAdsMonth ||
                                   keyConfigIndex == kKeyRowRemoveAdsYear ||
                                   keyConfigIndex == kKeyRowRemoveAdsForever) {
                            // Already paid: the rows render as a thank-you line
                            // with nothing to activate, so ignore the press
                            // rather than reopening Play on a purchase they
                            // already own.
                            if (!AdsRemoved()) {
                                int msg = keyConfigIndex == kKeyRowRemoveAdsMonth  ? 0x8004
                                        : keyConfigIndex == kKeyRowRemoveAdsYear   ? 0x8002
                                                                                   : 0x8003;
                                SDL_SendAndroidMessage(msg, 0);
                                AudioMixer::Instance()->PlaySFX("menu_selected");
                            }
#endif
                        } else if (keyConfigIndex == kKeyRowResetAll) {
                            // Two presses: the first arms, the second commits.
                            // This discards every key binding the player has
                            // set, and it sits directly below ordinary toggles,
                            // so one stray Enter must not be able to wipe them.
                            if (!resetAllArmed) {
                                resetAllArmed = true;
                                AudioMixer::Instance()->PlaySFX("menu_selected");
                                return true;
                            }
                            resetAllArmed = false;

                            GameSettings* gs = GameSettings::Instance();
                            gs->ResetToDefaults();

                            // Settings already pushed into a subsystem do not
                            // re-read themselves, so re-apply the two that were.
                            AudioMixer::Instance()->MuteAll(gs->soundEnabled());
                            SDL_SetWindowFullscreen(
                                SDL_GetRenderWindow(const_cast<SDL_Renderer*>(renderer)),
                                gs->fullscreenMode());

                            AudioMixer::Instance()->PlaySFX("typewriter");
                        } else {
                            // Wait for key press
                            AudioMixer::Instance()->PlaySFX("menu_selected");
                            awaitKp = true;
                        }
                        return true;
                    }
                }
            }
    return false;
}

bool MainMenu::LobbyChatTypingKey(SDL_Event *e) {
            // Handle text input for chat when Chat is selected in lobby
            if (showingNetPanel && networkInLobby && networkInputMode == 0 && selectedActionIndex == 0) {
                // Handle printable characters for chat input
                SDL_Keycode key = e->key.key;
                size_t len = strlen(networkChatInput);

                if ((key >= SDLK_A && key <= SDLK_Z) || (key >= SDLK_0 && key <= SDLK_9) ||
                    key == SDLK_SPACE || key == SDLK_EXCLAIM || key == SDLK_QUESTION ||
                    key == SDLK_COMMA || key == SDLK_PERIOD || key == SDLK_APOSTROPHE ||
                    key == SDLK_MINUS || key == SDLK_UNDERSCORE || key == SDLK_SLASH) {

                    if (len < sizeof(networkChatInput) - 1) {
                        char ch = (char)key;
                        // Convert to uppercase if shift is held
                        if (SDL_GetModState() & SDL_KMOD_SHIFT) {
                            if (ch >= 'a' && ch <= 'z') {
                                ch = ch - 'a' + 'A';
                            }
                        }
                        networkChatInput[len] = ch;
                        networkChatInput[len + 1] = '\0';
                    }
                    return true; // Don't process this key further
                }
            }
    return false;
}

// --- Game mode stepping (gamemode.h) -------------------------------------

void MainMenu::ModeValueLabel(char* out, size_t outSize, GameMode mode,
                              int raceTargetIndex, int timedSecondsIndex) const {
    if (out == nullptr || outSize == 0) return;
    if (mode == GameMode::Race) {
        snprintf(out, outSize, "%d bubbles", RaceTargetAt(raceTargetIndex));
    } else if (mode == GameMode::Timed) {
        const int secs = TimedSecondsAt(timedSecondsIndex);
        if (secs < 60) snprintf(out, outSize, "%d sec", secs);
        else snprintf(out, outSize, "%d:%02d", secs / 60, secs % 60);
    } else {
        out[0] = '\0';
    }
}

bool MainMenu::RoomRowHidden(int row) const {
    return row == kRoomModeValue && !GameModeCountsPops(netGameMode);
}

void MainMenu::StepLocalMPGameMode(bool forward) {
    const GameMode was = localMPGameMode;
    localMPGameMode = forward ? NextGameMode(was) : PrevGameMode(was);

    // Clear Mode forces row collapse off and attacks off, and is the only mode
    // that forces anything (GameModeForcesNoCompression). Snapshot on the way
    // in, restore on the way out -- keyed off whether the *mode* forces them
    // rather than off a Clear-specific comparison, so a future mode that
    // forces the same pair inherits this for free.
    const bool wasForced = GameModeForcesNoCompression(was);
    const bool nowForced = GameModeForcesNoCompression(localMPGameMode);
    if (nowForced && !wasForced) {
        localMPPreClearNoCompress = localMPNoCompress;
        localMPPreClearAttackMode = localMPAttackMode;
        localMPNoCompress = true;
        localMPAttackMode = AttackMode::Off;
    } else if (wasForced && !nowForced) {
        localMPNoCompress = localMPPreClearNoCompress;
        localMPAttackMode = localMPPreClearAttackMode;
    }

    // The list just grew or shrank by a row under the cursor. The cursor is on
    // the Mode row itself, which never moves, so nothing needs re-homing here
    // -- but the start row does move, and a stale index past it would leave
    // nothing highlighted.
    const int maxIdx = LocalMPStartRow(localMPPlayerCount, localMPGameMode);
    if (localMPMenuIndex > maxIdx) localMPMenuIndex = maxIdx;
}

void MainMenu::StepLocalMPModeValue(bool forward) {
    if (localMPGameMode == GameMode::Race) {
        localMPRaceTargetIndex = StepModeValueIndex(
            localMPRaceTargetIndex, (int)std::size(kRaceTargets), forward);
    } else if (localMPGameMode == GameMode::Timed) {
        localMPTimedSecondsIndex = StepModeValueIndex(
            localMPTimedSecondsIndex, (int)std::size(kTimedSeconds), forward);
    }
}

void MainMenu::StepNetGameMode(bool forward) {
    const GameMode was = netGameMode;
    netGameMode = forward ? NextGameMode(was) : PrevGameMode(was);

    const bool wasForced = GameModeForcesNoCompression(was);
    const bool nowForced = GameModeForcesNoCompression(netGameMode);
    if (nowForced && !wasForced) {
        for (int i = 0; i < 5; i++) netPreClearNoCompress[i] = playerNoCompress[i];
        netPreClearAttackMode = netAttackMode;
        for (int i = 0; i < 5; i++) playerNoCompress[i] = true;
        netAttackMode = AttackMode::Off;
    } else if (wasForced && !nowForced) {
        for (int i = 0; i < 5; i++) playerNoCompress[i] = netPreClearNoCompress[i];
        netAttackMode = netPreClearAttackMode;
    }
}

void MainMenu::StepNetModeValue(bool forward) {
    if (netGameMode == GameMode::Race) {
        netRaceTargetIndex = StepModeValueIndex(
            netRaceTargetIndex, (int)std::size(kRaceTargets), forward);
    } else if (netGameMode == GameMode::Timed) {
        netTimedSecondsIndex = StepModeValueIndex(
            netTimedSecondsIndex, (int)std::size(kTimedSeconds), forward);
    }
}

bool MainMenu::LocalMPPanelKey(SDL_Event *e) {
            if (showingLocalMPPanel && !runDelay) {
                // Row order and the per-player offsets live in
                // localmultiplayer_settings.h, shared with the panel that
                // draws them.
                int localMaxIdx = LocalMPStartRow(localMPPlayerCount, localMPGameMode);
                // The HELP box parks localMPMenuIndex on kLocalMPHelpTapIndex,
                // which is deliberately outside this list. Fold it back onto
                // the row the box sits on before stepping, or Up would walk
                // off the end and leave nothing highlighted. (Down already
                // wraps to 0 for any index past the end.)
                if (localMPMenuIndex > localMaxIdx) localMPMenuIndex = LocalMPRowBotSkill(localMPGameMode);
                if (e->key.key == SDLK_UP) {
                    localMPMenuIndex--;
                    if (localMPMenuIndex < 0) localMPMenuIndex = localMaxIdx;
                    AudioMixer::Instance()->PlaySFX("menu_change");
                    return true;
                } else if (e->key.key == SDLK_DOWN) {
                    localMPMenuIndex++;
                    if (localMPMenuIndex > localMaxIdx) localMPMenuIndex = 0;
                    AudioMixer::Instance()->PlaySFX("menu_change");
                    return true;
                } else if (e->key.key == SDLK_LEFT || e->key.key == SDLK_RIGHT) {
                    LocalMultiplayerMenuCommand command =
                        e->key.key == SDLK_LEFT
                            ? LocalMultiplayerMenuCommand::Left
                            : LocalMultiplayerMenuCommand::Right;
                    if (ApplyLocalMultiplayerVictoriesInput(
                            localMPMenuIndex, command,
                            localMPVictoriesIndex, localMPGameMode)) {
                        PlayMenuSFX("menu_change");
                        return true;
                    }
                    if (localMPMenuIndex == kLocalMPRowPlayers) {
                        if (e->key.key == SDLK_LEFT) {
                            localMPPlayerCount--;
                            if (localMPPlayerCount < kMinLocalPlayers)
                                localMPPlayerCount = kMaxLocalPlayers;
                        } else {
                            localMPPlayerCount++;
                            if (localMPPlayerCount > kMaxLocalPlayers)
                                localMPPlayerCount = kMinLocalPlayers;
                        }
                        // Fewer players can mean fewer seats for bots; leaving
                        // a stale count would start a game with no human in it.
                        localMPBotCount =
                            ClampLocalBotCount(localMPBotCount, localMPPlayerCount);
                        if (localMPMenuIndex > LocalMPStartRow(localMPPlayerCount, localMPGameMode))
                            localMPMenuIndex = LocalMPStartRow(localMPPlayerCount, localMPGameMode);
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex == 1) {
                        localMPCR = !localMPCR;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex == 2) {
                        localMPNoCompress = !localMPNoCompress;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex == kLocalMPRowMode) {
                        StepLocalMPGameMode(e->key.key != SDLK_LEFT);
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex == kLocalMPRowModeValue &&
                               GameModeCountsPops(localMPGameMode)) {
                        StepLocalMPModeValue(e->key.key != SDLK_LEFT);
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex == LocalMPRowMalus(localMPGameMode)) {
                        localMPAttackMode = e->key.key == SDLK_LEFT
                            ? PrevAttackMode(localMPAttackMode)
                            : NextAttackMode(localMPAttackMode);
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex == LocalMPRowTeam(localMPGameMode)) {
                        localMPTeamMode = !localMPTeamMode;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex == LocalMPRowBots(localMPGameMode)) {
                        AdjustLocalBotCount(
                            e->key.key == SDLK_LEFT
                                ? LocalMultiplayerMenuCommand::Left
                                : LocalMultiplayerMenuCommand::Right,
                            localMPBotCount, localMPPlayerCount);
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex == LocalMPRowBotSkill(localMPGameMode)) {
                        if (e->key.key == SDLK_LEFT) {
                            localMPBotSkill = localMPBotSkill <= 0 ? 2 : localMPBotSkill - 1;
                        } else {
                            localMPBotSkill = localMPBotSkill >= 2 ? 0 : localMPBotSkill + 1;
                        }
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex >= LocalMPFirstPlayerRow(localMPGameMode) &&
                               localMPMenuIndex < LocalMPAimGuideRow(localMPPlayerCount, localMPGameMode)) {
                        int pi = localMPMenuIndex - LocalMPFirstPlayerRow(localMPGameMode);
                        localMPAimGuide[pi] = !localMPAimGuide[pi];
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex >= LocalMPColorsRow(0, localMPPlayerCount, localMPGameMode) &&
                               localMPMenuIndex < LocalMPStartRow(localMPPlayerCount, localMPGameMode)) {
                        int pi = localMPMenuIndex - LocalMPColorsRow(0, localMPPlayerCount, localMPGameMode);
                        if (e->key.key == SDLK_LEFT) {
                            playerColorCounts[pi]--;
                            if (playerColorCounts[pi] < 5) playerColorCounts[pi] = 8;
                        } else {
                            playerColorCounts[pi]++;
                            if (playerColorCounts[pi] > 8) playerColorCounts[pi] = 5;
                        }
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    }
                    return true;
                } else if (e->key.key == SDLK_RETURN) {
                    if (ApplyLocalMultiplayerVictoriesInput(
                            localMPMenuIndex,
                            LocalMultiplayerMenuCommand::Enter,
                            localMPVictoriesIndex,
                            localMPGameMode)) {
                        PlayMenuSFX("menu_change");
                        return true;
                    }
                    if (localMPMenuIndex == kLocalMPRowPlayers) {
                        localMPPlayerCount++;
                        if (localMPPlayerCount > kMaxLocalPlayers)
                            localMPPlayerCount = kMinLocalPlayers;
                        localMPBotCount =
                            ClampLocalBotCount(localMPBotCount, localMPPlayerCount);
                        if (localMPMenuIndex > LocalMPStartRow(localMPPlayerCount, localMPGameMode))
                            localMPMenuIndex = LocalMPStartRow(localMPPlayerCount, localMPGameMode);
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex == 1) {
                        localMPCR = !localMPCR;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex == 2) {
                        localMPNoCompress = !localMPNoCompress;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex == kLocalMPRowMode) {
                        StepLocalMPGameMode(true);
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex == kLocalMPRowModeValue &&
                               GameModeCountsPops(localMPGameMode)) {
                        StepLocalMPModeValue(true);
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex == LocalMPRowMalus(localMPGameMode)) {
                        localMPAttackMode = NextAttackMode(localMPAttackMode);
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex == LocalMPRowTeam(localMPGameMode)) {
                        localMPTeamMode = !localMPTeamMode;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex == LocalMPRowBots(localMPGameMode)) {
                        AdjustLocalBotCount(LocalMultiplayerMenuCommand::Enter,
                                            localMPBotCount, localMPPlayerCount);
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex == LocalMPRowBotSkill(localMPGameMode)) {
                        localMPBotSkill = localMPBotSkill >= 2 ? 0 : localMPBotSkill + 1;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex >= LocalMPFirstPlayerRow(localMPGameMode) &&
                               localMPMenuIndex < LocalMPAimGuideRow(localMPPlayerCount, localMPGameMode)) {
                        int pi = localMPMenuIndex - LocalMPFirstPlayerRow(localMPGameMode);
                        localMPAimGuide[pi] = !localMPAimGuide[pi];
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex >= LocalMPColorsRow(0, localMPPlayerCount, localMPGameMode) &&
                               localMPMenuIndex < LocalMPStartRow(localMPPlayerCount, localMPGameMode)) {
                        int pi = localMPMenuIndex - LocalMPColorsRow(0, localMPPlayerCount, localMPGameMode);
                        playerColorCounts[pi]++;
                        if (playerColorCounts[pi] > 8) playerColorCounts[pi] = 5;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex == localMaxIdx) {
                        // Start game!
                        AudioMixer::Instance()->PlaySFX("menu_selected");
                        delayTime = 60;
                        runDelay = true;
                    }
                    return true;
                } else if (e->key.key == SDLK_ESCAPE) {
                    showingLocalMPPanel = false;
                    AudioMixer::Instance()->PlaySFX("menu_change");
                    return true;
                }
            }
    return false;
}

// Menu sound effect that is safe to call from a headless test, where no audio
// device exists. Was PlayLocalMPMenuSFX; renamed once the panel-tap handler
// needed the same guard to be testable.
void MainMenu::PlayMenuSFX(const char *name) {
#ifdef FROZEN_BUBBLE_TEST_ACCESS
    if (headlessTestMode) return;
#endif
    AudioMixer::Instance()->PlaySFX(name);
}

void MainMenu::MenuUpKey() {
                    // LAN menu navigation (0 = Set Name, 1 = Host, 2+ = servers)
                    if (showingNetPanel && !networkInLobby && networkInputMode == 7) {
                        if (lanMenuIndex > 0) { lanMenuIndex--; AudioMixer::Instance()->PlaySFX("menu_change"); }
                        return;
                    }
                    // Net game menu navigation (0 = Manual entry, 1..n = public servers, n+1 = Set Name)
                    if (showingNetPanel && !networkInLobby && networkInputMode == 10) {
                        if (netMenuIndex > 0) { netMenuIndex--; AudioMixer::Instance()->PlaySFX("menu_change"); }
                        return;
                    }
                    // Handle action menu selection in network lobby
                    if (showingNetPanel && networkInLobby && networkInputMode == 0) {
                        NetworkClient* netClient = NetworkClient::Instance();
                        if (netClient->GetState() == CONNECTED || netClient->GetState() == IN_LOBBY) {
                            GameRoom* currentGame = netClient->GetCurrentGame();
                            bool isHostRoom = currentGame && currentGame->creator == netClient->GetPlayerNick();

                            int maxActions;
                            if (currentGame && isHostRoom) {
                                // Host always sees Bots/BotSkill (kRoomBotSkill is the last
                                // row before the optional Start one), which this hand-counted
                                // total used to leave out entirely -- Down from Bot skill
                                // wrapped straight back to Chat instead of ever reaching
                                // "Start game!" (found live: could not tab to Start with
                                // arrow keys). Derived from the same row enum
                                // NetPanelLobbyActionsRender builds the list from instead of
                                // a second hand count, so the two can't drift apart again.
                                maxActions = kRoomBotSkill + 1 + ((int)currentGame->players.size() > 1 ? 1 : 0);
                            } else if (currentGame) {
                                // Joiner: navigate the same rows host does through
                                // kRoomTeam, minus the host-only Bots/BotSkill/Start ones.
                                maxActions = kRoomTeam + 1;
                            } else {
                                // In lobby
                                std::vector<GameRoom> games = netClient->GetGameList();
                                maxActions = 2 + games.size(); // Chat + Create + Join games
                                // ...plus the "Join our Discord" row the lobby
                                // list draws after the rooms, when one is
                                // compiled in. Without this it renders but
                                // arrow keys stop one short of it.
                                if (LobbyDiscordIndex(games.size()) >= 0) maxActions++;
                            }
                            // The HELP box (kRoomHelpTapIndex) and the >5-cap
                            // roster's tap rows park selectedActionIndex on fake
                            // indices deliberately outside this list. Fold those
                            // back to the last real row first, or stepping from
                            // one would walk off the end and leave nothing
                            // highlighted. The header's "Set Teams" button
                            // (kRoomSetTeamsTapIndex) and the HELP box are the
                            // exceptions -- both are now real stops in the cycle
                            // below -- and only while a room actually exists to
                            // show them; a stale 950/952 after leaving the room
                            // folds like the rest, or it would strand the lobby
                            // cursor.
                            if (selectedActionIndex >= maxActions &&
                                !(currentGame &&
                                  (selectedActionIndex == kRoomSetTeamsTapIndex ||
                                   selectedActionIndex == kRoomHelpTapIndex)))
                                selectedActionIndex = maxActions - 1;

                            if (currentGame) {
                                if (selectedActionIndex == kRoomSetTeamsTapIndex) {
                                    // "Set Teams" is the header button, so it sits
                                    // above Chat in the cycle: Up from it goes to
                                    // HELP, then the last real row.
                                    selectedActionIndex = kRoomHelpTapIndex;
                                } else if (selectedActionIndex == kRoomHelpTapIndex) {
                                    // HELP sits between the last real row and
                                    // Set Teams -- previously unreachable by
                                    // Up/Down at all (tap or F1 only). Drawn for
                                    // host and joiner alike, so it is a stop for
                                    // both.
                                    selectedActionIndex = maxActions - 1;
                                } else {
                                    selectedActionIndex--;
                            // kRoomModeValue is in the enum but off screen
                            // in Classic and Clear, so step past it rather
                            // than parking the highlight on a row that isn't
                            // drawn. One extra step always lands on a real row:
                            // the hidden row is neither the first nor the last
                            // in the list, so it can never be reached twice in
                            // a row nor push the cursor off either end.
                                    if (RoomRowHidden(selectedActionIndex))
                                        selectedActionIndex--;
                                    if (selectedActionIndex < 0)
                                        selectedActionIndex = kRoomSetTeamsTapIndex;
                                }
                            } else {
                                selectedActionIndex--;
                                if (selectedActionIndex < 0) selectedActionIndex = maxActions - 1;
                            }
                            AudioMixer::Instance()->PlaySFX("menu_change");

                            // Joiner on Teams row: auto-focus their own column
                            if (currentGame && !isHostRoom && selectedActionIndex == kRoomTeam) {
                                std::string myNick = netClient->GetPlayerNick();
                                for (int i = 0; i < (int)currentGame->players.size(); i++) {
                                    if (currentGame->players[i].nick == myNick) { currentPlayerCol = i + 1; break; }
                                }
                            }
                        }
                    } else {
                        up();
                    }
                    return;
}

void MainMenu::MenuDownKey() {
                    // LAN menu navigation
                    if (showingNetPanel && !networkInLobby && networkInputMode == 7) {
                        int lanMenuMax = 2 + (int)discoveredServers.size(); // 0=Host, 1..n=servers, n+1=SetName
                        if (lanMenuIndex < lanMenuMax - 1) { lanMenuIndex++; AudioMixer::Instance()->PlaySFX("menu_change"); }
                        return;
                    }
                    // Net game menu navigation
                    if (showingNetPanel && !networkInLobby && networkInputMode == 10) {
                        // 0=Manual, 1..n=servers, n+1=SetName, and n+2=Join our
                        // Discord when an invite is compiled in -- without the
                        // extra stop, keyboard and gamepad players could see
                        // the Discord row but never reach it, which is exactly
                        // the input-parity gap CLAUDE.md calls out.
                        int netMenuMax = 2 + (int)publicServers.size();
                        if (ServerListDiscordIndex() >= 0) netMenuMax++;
                        if (netMenuIndex < netMenuMax - 1) { netMenuIndex++; AudioMixer::Instance()->PlaySFX("menu_change"); }
                        return;
                    }
                    // Handle action menu selection in network lobby
                    if (showingNetPanel && networkInLobby && networkInputMode == 0) {
                        NetworkClient* netClient = NetworkClient::Instance();
                        if (netClient->GetState() == CONNECTED || netClient->GetState() == IN_LOBBY) {
                            GameRoom* currentGame = netClient->GetCurrentGame();
                            bool isHostRoom = currentGame && currentGame->creator == netClient->GetPlayerNick();

                            int maxActions;
                            if (currentGame && isHostRoom) {
                                // Host always sees Bots/BotSkill (kRoomBotSkill is the last
                                // row before the optional Start one), which this hand-counted
                                // total used to leave out entirely -- Down from Bot skill
                                // wrapped straight back to Chat instead of ever reaching
                                // "Start game!" (found live: could not tab to Start with
                                // arrow keys). Derived from the same row enum
                                // NetPanelLobbyActionsRender builds the list from instead of
                                // a second hand count, so the two can't drift apart again.
                                maxActions = kRoomBotSkill + 1 + ((int)currentGame->players.size() > 1 ? 1 : 0);
                            } else if (currentGame) {
                                // Joiner: navigate the same rows host does through
                                // kRoomTeam, minus the host-only Bots/BotSkill/Start ones.
                                maxActions = kRoomTeam + 1;
                            } else {
                                // In lobby
                                std::vector<GameRoom> games = netClient->GetGameList();
                                maxActions = 2 + games.size(); // Chat + Create + Join games
                                // ...plus the "Join our Discord" row the lobby
                                // list draws after the rooms, when one is
                                // compiled in. Without this it renders but
                                // arrow keys stop one short of it.
                                if (LobbyDiscordIndex(games.size()) >= 0) maxActions++;
                            }
                            if (currentGame) {
                                if (selectedActionIndex == kRoomSetTeamsTapIndex) {
                                    // Down from the header button wraps to Chat.
                                    selectedActionIndex = 0;
                                } else if (selectedActionIndex == kRoomHelpTapIndex) {
                                    // Down from HELP continues to the header's
                                    // Set Teams button.
                                    selectedActionIndex = kRoomSetTeamsTapIndex;
                                } else if (selectedActionIndex >= maxActions) {
                                    // Parked on a >5-cap roster row: wrap to the
                                    // top as before.
                                    selectedActionIndex = 0;
                                } else {
                                    selectedActionIndex++;
                            // kRoomModeValue is in the enum but off screen
                            // in Classic and Clear, so step past it rather
                            // than parking the highlight on a row that isn't
                            // drawn. One extra step always lands on a real row:
                            // the hidden row is neither the first nor the last
                            // in the list, so it can never be reached twice in
                            // a row nor push the cursor off either end.
                                    if (RoomRowHidden(selectedActionIndex))
                                        selectedActionIndex++;
                                    if (selectedActionIndex >= maxActions)
                                        selectedActionIndex = kRoomHelpTapIndex;
                                }
                            } else {
                                selectedActionIndex++;
                                if (selectedActionIndex >= maxActions) selectedActionIndex = 0;
                            }
                            AudioMixer::Instance()->PlaySFX("menu_change");

                            // Joiner on Teams row: auto-focus their own column
                            if (currentGame && !isHostRoom && selectedActionIndex == kRoomTeam) {
                                std::string myNick = netClient->GetPlayerNick();
                                for (int i = 0; i < (int)currentGame->players.size(); i++) {
                                    if (currentGame->players[i].nick == myNick) { currentPlayerCol = i + 1; break; }
                                }
                            }
                        }
                    } else {
                        down();
                    }
                    return;
}

void MainMenu::MenuLeftRightKey(SDL_Event *e) {
                    // Handle LEFT/RIGHT for changing settings values (when not Chat)
                    if (showingNetPanel && networkInLobby && networkInputMode == 0 && selectedActionIndex != 0) {
                        NetworkClient* netClient = NetworkClient::Instance();
                        GameRoom* currentGame = netClient->GetCurrentGame();
                        if (currentGame && currentGame->creator != netClient->GetPlayerNick() && selectedActionIndex == kRoomTeam) {
                            // Joiner LEFT/RIGHT on Teams row: change own team and notify host
                            std::string myNick = netClient->GetPlayerNick();
                            int mySlot = -1;
                            for (int i = 0; i < (int)currentGame->players.size(); i++) {
                                if (currentGame->players[i].nick == myNick) { mySlot = i; break; }
                            }
                            if (mySlot >= 0) {
                                netPlayerTeams[mySlot] = StepTeamChoice(
                                    netPlayerTeams[mySlot], kMaxTeams,
                                    e->key.key == SDLK_LEFT ? -1 : 1);
                                AudioMixer::Instance()->PlaySFX("menu_change");
                                char talkMsg[32];
                                snprintf(talkMsg, sizeof(talkMsg), "!team:%s:%d", myNick.c_str(), netPlayerTeams[mySlot]);
                                netClient->SendTalk(talkMsg);
                            }
                        } else if (currentGame && currentGame->creator == netClient->GetPlayerNick()) {
                            // Only host can change settings
                            bool settingChanged = false;
                            if (selectedActionIndex == kRoomMode) {
                                // Four states now, so Left has to walk back
                                // rather than forward -- same as kRoomMalus
                                // below, and unlike the plain toggles.
                                StepNetGameMode(e->key.key != SDLK_LEFT);
                                AudioMixer::Instance()->PlaySFX("menu_change");
                                settingChanged = true;
                            } else if (selectedActionIndex == kRoomModeValue &&
                                       !RoomRowHidden(kRoomModeValue)) {
                                StepNetModeValue(e->key.key != SDLK_LEFT);
                                AudioMixer::Instance()->PlaySFX("menu_change");
                                settingChanged = true;
                            } else if (selectedActionIndex == kRoomMalus) {
                                // Three states now, so Left has to walk back
                                // rather than forward -- same as kRoomMode
                                // above, and unlike the plain toggles below.
                                netAttackMode = (e->key.key == SDLK_LEFT)
                                    ? PrevAttackMode(netAttackMode)
                                    : NextAttackMode(netAttackMode);
                                AudioMixer::Instance()->PlaySFX("menu_change");
                                settingChanged = true;
                            } else if (selectedActionIndex == kRoomChain) {
                                chainReactionEnabled = !chainReactionEnabled;
                                AudioMixer::Instance()->PlaySFX("menu_change");
                                settingChanged = true;
                            } else if (selectedActionIndex == kRoomTarget) {
                                singlePlayerTargetting = !singlePlayerTargetting;
                                AudioMixer::Instance()->PlaySFX("menu_change");
                                settingChanged = true;
                            } else if (selectedActionIndex == kRoomVictories) {
                                // LEFT/RIGHT cycle victories limit
                                if (e->key.key == SDLK_LEFT) {
                                    victoriesLimitIndex--;
                                    if (victoriesLimitIndex < 0) victoriesLimitIndex = 17;
                                } else {
                                    victoriesLimitIndex++;
                                    if (victoriesLimitIndex > 17) victoriesLimitIndex = 0;
                                }
                                AudioMixer::Instance()->PlaySFX("menu_change");
                                settingChanged = true;
                            } else if (selectedActionIndex == kRoomBots) {
                                // Each bot is a real seat in the room, so the
                                // ceiling is what the room has left.
                                const int maxBots = MaxRoomBots(
                                    (int)currentGame->players.size(),
                                    currentGame->maxPlayers, netRoomBotCount);
                                netRoomBotCount += (e->key.key == SDLK_LEFT) ? -1 : 1;
                                if (netRoomBotCount < 0) netRoomBotCount = maxBots;
                                if (netRoomBotCount > maxBots) netRoomBotCount = 0;
                                SyncLobbyBots();
                                AudioMixer::Instance()->PlaySFX("menu_change");
                            } else if (selectedActionIndex == kRoomBotSkill) {
                                netRoomBotSkill += (e->key.key == SDLK_LEFT) ? -1 : 1;
                                if (netRoomBotSkill < 0) netRoomBotSkill = 2;
                                if (netRoomBotSkill > 2) netRoomBotSkill = 0;
                                SaveHostDefaults();
                                AudioMixer::Instance()->PlaySFX("menu_change");
                            } else if (selectedActionIndex >= kRoomGridFirst &&
                                       selectedActionIndex <= kRoomGridLast) {
                                // Grid rows: Left/Right navigates player columns
                                // col 0 = ALL, col 1..N = P1..PN
                                int numPlayers = (int)currentGame->players.size();
                                if (numPlayers < 1) numPlayers = 1;
                                if (numPlayers > 5) numPlayers = 5;
                                int totalCols = numPlayers + 1; // +1 for ALL
                                if (e->key.key == SDLK_LEFT) {
                                    currentPlayerCol--;
                                    if (currentPlayerCol < 0) currentPlayerCol = totalCols - 1;
                                } else {
                                    currentPlayerCol++;
                                    if (currentPlayerCol >= totalCols) currentPlayerCol = 0;
                                }
                                AudioMixer::Instance()->PlaySFX("menu_change");
                            } else if (selectedActionIndex == kRoomTeam) {
                                // Team row: Left/Right changes team number for focused column
                                int numPlayers = (int)currentGame->players.size();
                                if (numPlayers < 1) numPlayers = 1;
                                if (numPlayers > 5) numPlayers = 5;
                                int lo = (currentPlayerCol == 0) ? 0 : currentPlayerCol - 1;
                                int hi = (currentPlayerCol == 0) ? numPlayers : currentPlayerCol;
                                for (int i = lo; i < hi; i++) {
                                    netPlayerTeams[i] = StepTeamChoice(
                                        netPlayerTeams[i], kMaxTeams,
                                        e->key.key == SDLK_LEFT ? -1 : 1);
                                }
                                AudioMixer::Instance()->PlaySFX("menu_change");
                                settingChanged = true;
                            }
                            if (settingChanged) {
                                SyncRoomOptions();
                            }
                        } else if (!currentGame && selectedActionIndex == 1) {
                            // Lobby "Create Game Room" row: Left/Right cycles the room-size choice
                            if (e->key.key == SDLK_LEFT) {
                                netRoomSizeChoice--;
                                if (netRoomSizeChoice < 0) netRoomSizeChoice = 2;
                            } else {
                                netRoomSizeChoice++;
                                if (netRoomSizeChoice > 2) netRoomSizeChoice = 0;
                            }
                            SaveHostDefaults();
                            AudioMixer::Instance()->PlaySFX("menu_change");
                        }
                    }
                    return;
}

void MainMenu::SubmitLobbyChatInput(NetworkClient *netClient) {
#ifdef __WASM_PORT__
    // Touch devices can't type into the inline chat field
    // (SDL3's Emscripten backend can't summon the soft
    // keyboard) — compose in a native browser prompt.
    if (WasmHasTouch() && networkChatInput[0] == '\0') {
        WasmPromptText("Chat message:", "", networkChatInput, sizeof(networkChatInput));
    }
#endif
#if defined(__IOS_PORT__) || defined(__ANDROID__)
    // Neither touch nor a TV remote has a keyboard until something asks for
    // one, and activating the chat row never did: it went straight to "send",
    // which with an empty field does nothing at all -- so the row simply
    // appeared dead.
    //
    // Hand over to the dedicated chat input mode instead. It already raises the
    // system keyboard and routes SDL_EVENT_TEXT_INPUT into this same buffer,
    // which typing into the row does not: that path reads key events, and both
    // a soft keyboard and a TV's on-screen keyboard report characters as text
    // input, not keystrokes. Enter there sends and comes back here.
    //
    // Deliberately not gated on DeviceHasTouchscreen(): a TV box needs this
    // handover just as much as a phone, having no keyboard either, and the
    // lobby's T-to-chat shortcut is a key a remote does not have. The gate used
    // to be here and was harmless only because that predicate answered true on
    // every Android device; once it learned to say false on a TV, it took TV
    // chat with it.
    if (networkChatInput[0] == '\0') {
        networkInputMode = 4;
        SDL_StopTextInput(SDL_GetKeyboardFocus());
        SDL_StartTextInput(SDL_GetKeyboardFocus());
        SetTextInputAreaLogical(const_cast<SDL_Renderer*>(renderer), {160, 152, 320, 20});
        AudioMixer::Instance()->PlaySFX("menu_selected");
        return;
    }
#endif
    // Chat - handle commands or send message
    if (strlen(networkChatInput) > 0) {
        // Check if it's a command (starts with /)
        if (networkChatInput[0] == '/') {
            // Handle chat commands
            if (strncmp(networkChatInput, "/nick ", 6) == 0) {
                // Change nickname
                const char* newNick = networkChatInput + 6;
                if (strlen(newNick) > 0 && netClient->SendNick(newNick)) {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "You are now known as %s", newNick);
                    netClient->AddStatusMessage(msg);
                } else {
                    netClient->AddStatusMessage("Erroneous nickname");
                }
            } else if (strncmp(networkChatInput, "/block ", 7) == 0) {
                // Hide a player's chat on this device. Local and immediate --
                // it needs no server support, so it works even against a
                // server with no moderation of any kind.
                // Trimmed and clamped to the length the server will actually
                // report back; matching below is case-insensitive.
                std::string target = GameSettings::NormalizeNick(networkChatInput + 7);
                GameSettings* gs = GameSettings::Instance();
                char msg[160];
                if (target.empty()) {
                    netClient->AddStatusMessage("Usage: /block <nick>");
                } else if (GameSettings::NicksEqual(target, netClient->GetPlayerNick())) {
                    netClient->AddStatusMessage("You cannot block yourself");
                } else if (gs->IsPlayerBlocked(target)) {
                    netClient->AddStatusMessage("Already blocked. Use /unblock to undo.");
                } else if (gs->ToggleBlockedPlayer(target)) {
                    // SaveKeys(), not SaveSettings(): only SaveKeys() refreshes
                    // the Blocked:* ini keys from the in-memory list before
                    // writing. Saving the other way persisted everything except
                    // the block that was just made.
                    gs->SaveKeys();
                    snprintf(msg, sizeof(msg), "Blocked %s -- their chat is now hidden", target.c_str());
                    netClient->AddStatusMessage(msg);
                } else {
                    // Only remaining failure is the bounded-list cap.
                    snprintf(msg, sizeof(msg), "Block list is full (%d) -- /unblock someone first",
                             GameSettings::kMaxBlockedPlayers);
                    netClient->AddStatusMessage(msg);
                }
            } else if (strncmp(networkChatInput, "/unblock ", 9) == 0) {
                std::string target = GameSettings::NormalizeNick(networkChatInput + 9);
                GameSettings* gs = GameSettings::Instance();
                char msg[160];
                if (target.empty()) {
                    netClient->AddStatusMessage("Usage: /unblock <nick>");
                } else if (!gs->IsPlayerBlocked(target)) {
                    netClient->AddStatusMessage("That player is not blocked");
                } else {
                    gs->ToggleBlockedPlayer(target);   // known present: removes
                    gs->SaveKeys();                    // see /block above
                    snprintf(msg, sizeof(msg), "Unblocked %s", target.c_str());
                    netClient->AddStatusMessage(msg);
                }
            } else if (strcmp(networkChatInput, "/blocked") == 0) {
                const GameSettings* gs = GameSettings::Instance();
                if (gs->blockedPlayers.empty()) {
                    netClient->AddStatusMessage("You have not blocked anyone");
                } else {
                    std::string list = "Blocked:";
                    for (const std::string& n : gs->blockedPlayers) list += " " + n;
                    netClient->AddStatusMessage(list);
                }
            } else if (strncmp(networkChatInput, "/report ", 8) == 0) {
                // Sends the report to the server operator. Deliberately says
                // "sent to the server operator" rather than implying anything
                // happens automatically -- nothing does, and promising
                // otherwise would be worse than saying nothing.
                const char* rest = networkChatInput + 8;
                while (*rest == ' ') rest++;
                char nickBuf[64];
                const char* space = strchr(rest, ' ');
                char msg[192];
                if (*rest == '\0' || space == nullptr || *(space + 1) == '\0') {
                    netClient->AddStatusMessage("Usage: /report <nick> <what happened>");
                } else {
                    size_t nickLen = (size_t)(space - rest);
                    if (nickLen >= sizeof(nickBuf)) nickLen = sizeof(nickBuf) - 1;
                    memcpy(nickBuf, rest, nickLen);
                    nickBuf[nickLen] = '\0';
                    const char* reason = space + 1;
                    if (netClient->SendReport(nickBuf, reason)) {
                        snprintf(msg, sizeof(msg),
                                 "Reported %s to the server operator. Use /block %s to hide their chat.",
                                 nickBuf, nickBuf);
                        netClient->AddStatusMessage(msg);
                    } else {
                        netClient->AddStatusMessage("Could not send the report");
                    }
                }
            } else if (strncmp(networkChatInput, "/kick ", 6) == 0) {
                // Host-only removal. The server enforces that (game.c only
                // honours KICK from the room's creator); the checks here are
                // so the host gets a useful message instead of a bare
                // protocol error, not as the security boundary.
                const char* rest = networkChatInput + 6;
                while (*rest == ' ') rest++;
                GameRoom* room = netClient->GetCurrentGame();
                if (!room) {
                    netClient->AddStatusMessage("You are not in a game room");
                } else if (room->creator != netClient->GetPlayerNick()) {
                    netClient->AddStatusMessage("Only the host can remove players");
                } else if (*rest == '\0') {
                    netClient->AddStatusMessage("Usage: /kick p2  (or /kick <nick>)");
                } else {
                    // "p2" means the second row of the roster, which is what
                    // the player is actually looking at -- nicknames are
                    // truncated to ten characters and easy to mistype.
                    std::string target = rest;
                    // A trailing space is invisible in the chat box and would
                    // otherwise turn "p2 " into a nickname lookup that can
                    // never match -- the same trap /block once fell into.
                    while (!target.empty() && (target.back() == ' ' || target.back() == '\t'))
                        target.pop_back();
                    if (!target.empty() &&
                        (target[0] == 'p' || target[0] == 'P') && target.size() > 1 &&
                        target.find_first_not_of("0123456789", 1) == std::string::npos) {
                        const int slot = std::atoi(target.c_str() + 1);
                        if (slot < 1 || slot > (int)room->players.size()) {
                            char msg[96];
                            snprintf(msg, sizeof(msg), "There is no player %s in this room",
                                     target.c_str());
                            netClient->AddStatusMessage(msg);
                            target.clear();
                        } else {
                            target = room->players[slot - 1].nick;
                        }
                    }
                    if (!target.empty()) {
                        char msg[128];
                        if (target == netClient->GetPlayerNick()) {
                            netClient->AddStatusMessage("You cannot remove yourself -- press ESC to leave");
                        } else if (netClient->KickPlayer(target.c_str())) {
                            snprintf(msg, sizeof(msg), "Removing %s...", target.c_str());
                            netClient->AddStatusMessage(msg);
                        } else {
                            netClient->AddStatusMessage("Could not send the kick");
                        }
                    }
                }
            } else if (strcmp(networkChatInput, "/help") == 0) {
                // Show help
                netClient->AddStatusMessage("Commands: /nick <new_nick>, /kick p2|<nick> (host), /block <nick>, /unblock <nick>, /blocked, /report <nick> <reason>, /help");
            } else {
                // Unknown command
                netClient->AddStatusMessage("Unknown command. Type /help for help.");
            }
        } else {
            // Regular chat message - send it
            netClient->SendTalk(networkChatInput);
        }
        networkChatInput[0] = '\0';
        AudioMixer::Instance()->PlaySFX("menu_selected");
    }
}

void MainMenu::GameRoomHostReturn(NetworkClient *netClient, GameRoom *currentGame) {
    // Teams are set on their own full-screen page (mainmenu_teampanel.cpp),
    // opened by the [A] hotkey in HandleInput or by activating the Team row
    // below -- not from a dedicated action row of its own, which never fit
    // above the persistent chat dock.
    // Row indices are the GameRoomRow enum in mainmenu_internal.h -- the list
    // is built positionally in mainmenu_netpanel.cpp and acted on by index
    // here, so the two have to agree.
    int numPlayers = currentGame ? (int)currentGame->players.size() : 1;
    if (numPlayers < 1) numPlayers = 1;
    if (numPlayers > 5) numPlayers = 5;
    bool settingChanged = false;
    if (selectedActionIndex == kRoomMode) {
        // Cycle game mode: Classic -> Clear -> Race -> Timed.
        //
        // "Teams" used to be the third stop here, which made team play a
        // property of the whole match: you could either have teams or Clear
        // Mode, never both, and Classic could never have them at all. Teams
        // are a per-player setting now (the Set Teams page), available in
        // every mode, so the mode row is back to naming just the ruleset.
        AudioMixer::Instance()->PlaySFX("menu_change");
        StepNetGameMode(true);
        settingChanged = true;
    } else if (selectedActionIndex == kRoomModeValue && !RoomRowHidden(kRoomModeValue)) {
        AudioMixer::Instance()->PlaySFX("menu_change");
        StepNetModeValue(true);
        settingChanged = true;
    } else if (selectedActionIndex == kRoomMalus) {
        netAttackMode = NextAttackMode(netAttackMode);
        AudioMixer::Instance()->PlaySFX("menu_change");
        settingChanged = true;
    } else if (selectedActionIndex == kRoomChain) {
        // Toggle chain reaction
        chainReactionEnabled = !chainReactionEnabled;
        AudioMixer::Instance()->PlaySFX("menu_change");
        settingChanged = true;
    } else if (selectedActionIndex == kRoomTarget) {
        // Toggle single player targetting
        singlePlayerTargetting = !singlePlayerTargetting;
        AudioMixer::Instance()->PlaySFX("menu_change");
        settingChanged = true;
    } else if (selectedActionIndex == kRoomVictories) {
        // Cycle victories limit
        victoriesLimitIndex++;
        if (victoriesLimitIndex > 17) victoriesLimitIndex = 0; // 18 values total
        AudioMixer::Instance()->PlaySFX("menu_change");
        settingChanged = true;
    } else if (selectedActionIndex == kRoomMouse) {
        // Toggle mouse/touch aim (per-session, off by default)
        netRoomMouseEnabled = !netRoomMouseEnabled;
        AudioMixer::Instance()->PlaySFX("menu_change");
    } else if (selectedActionIndex == kRoomMaxColors) {
        // Cycle per-player color count; col 0 = ALL
        int np = (int)currentGame->players.size();
        if (np < 1) np = 1; if (np > 5) np = 5;
        int lo = (currentPlayerCol == 0) ? 0 : currentPlayerCol - 1;
        int hi = (currentPlayerCol == 0) ? np : currentPlayerCol;
        for (int i = lo; i < hi; i++) {
            playerColorCounts[i]++;
            if (playerColorCounts[i] > 8) playerColorCounts[i] = 5;
        }
        AudioMixer::Instance()->PlaySFX("menu_change");
        settingChanged = true;
    } else if (selectedActionIndex == kRoomRows) {
        // Toggle per-player compression; col 0 = ALL (set all to majority opposite)
        int np = (int)currentGame->players.size();
        if (np < 1) np = 1; if (np > 5) np = 5;
        int lo = (currentPlayerCol == 0) ? 0 : currentPlayerCol - 1;
        int hi = (currentPlayerCol == 0) ? np : currentPlayerCol;
        bool allOn = true;
        for (int i = lo; i < hi; i++) if (playerNoCompress[i]) allOn = false;
        for (int i = lo; i < hi; i++) playerNoCompress[i] = allOn;
        AudioMixer::Instance()->PlaySFX("menu_change");
        settingChanged = true;
    } else if (selectedActionIndex == kRoomAim) {
        // Toggle per-player aim guide; col 0 = ALL (set all to majority opposite)
        int np = (int)currentGame->players.size();
        if (np < 1) np = 1; if (np > 5) np = 5;
        int lo = (currentPlayerCol == 0) ? 0 : currentPlayerCol - 1;
        int hi = (currentPlayerCol == 0) ? np : currentPlayerCol;
        bool allOn = true;
        for (int i = lo; i < hi; i++) if (!playerAimGuide[i]) allOn = false;
        for (int i = lo; i < hi; i++) playerAimGuide[i] = !allOn;
        AudioMixer::Instance()->PlaySFX("menu_change");
        settingChanged = true;
    } else if (selectedActionIndex == kRoomTeam) {
        // ENTER opens the full-screen team picker on the focused column's
        // player (mainmenu_teampanel.cpp), rather than advancing that cell's
        // team by one the way it used to. LEFT/RIGHT still cycle in place for
        // anyone who prefers it; what ENTER gains is the ability to pick a
        // team outright, and -- since a second tap on a grid cell is what
        // pushes RETURN here -- a touch path to the page at all. These cells
        // are 18 logical units tall, so cycling to team 4 by touch meant
        // hitting the same near-invisible target three times.
        OpenTeamsPanel(currentPlayerCol > 0 ? currentPlayerCol - 1 : -1);
    } else if (selectedActionIndex == kRoomBots && currentGame) {
        // ENTER: add one more bot, wrapping at the room's ceiling (same as RIGHT)
        const int maxBots = MaxRoomBots((int)currentGame->players.size(),
                                        currentGame->maxPlayers, netRoomBotCount);
        netRoomBotCount = (netRoomBotCount >= maxBots) ? 0 : netRoomBotCount + 1;
        SyncLobbyBots();
        AudioMixer::Instance()->PlaySFX("menu_change");
    } else if (selectedActionIndex == kRoomBotSkill) {
        netRoomBotSkill = (netRoomBotSkill >= 2) ? 0 : netRoomBotSkill + 1;
        SaveHostDefaults();
        AudioMixer::Instance()->PlaySFX("menu_change");
    } else if (selectedActionIndex == kRoomStart && currentGame && currentGame->players.size() > 1
               && !netStartRequested) {
        // Start game. Guarded on netStartRequested (distinct from
        // networkGameStarting, which latches once the transition into the
        // game itself begins -- reusing that one here would suppress the
        // transition entirely once GAME_CAN_START finally arrived) so
        // mashing this row while waiting on the server's reply -- a
        // round-trip, unlike a local game's instant start -- doesn't resend
        // "START" once per extra tap. Harmless to the server, but each such
        // tap is also a stray click left sitting in the input queue that
        // could otherwise be delivered on the very first frame of the game
        // that follows, misread there as a shot (see the queue flush in
        // BubbleGame::NewGame()). Times out in NetPanelRender() so a
        // rejected or dropped START (e.g. the room emptied out in the
        // meantime) doesn't lock this row forever.
        netStartRequested = true;
        netStartRequestedMs = SDL_GetTicks();
        netClient->StartGame();
        netClient->AddStatusMessage("Starting game...");
        AudioMixer::Instance()->PlaySFX("menu_selected");
    }
    if (settingChanged) {
        SyncRoomOptions();
    }
}

void MainMenu::MenuReturnKey() {
                    if (showingLevelPanel && !runDelay) {
                        // Confirm level selection
                        if (!levelInput.empty()) {
                            int lv = std::stoi(levelInput);
                            if (lv < 1) lv = 1;
                            if (lv > 100) lv = 100;
                            pickedStartLevel = lv;
                        }
                        AudioMixer::Instance()->PlaySFX("menu_selected");
                        delayTime = 60;
                        runDelay = true;
                        return;
                    }
                    if (showingNetPanel && networkInLobby && networkInputMode == 0) {
                        // Execute selected action in main lobby or game room
                        NetworkClient* netClient = NetworkClient::Instance();
                        if (netClient->GetState() == CONNECTED || netClient->GetState() == IN_LOBBY) {
                            GameRoom* currentGame = netClient->GetCurrentGame();

                            if (selectedActionIndex == 0) {
                                SubmitLobbyChatInput(netClient);
                            } else if (currentGame) {
                                // In game room - handle game settings and Start/Part actions
                                bool isHost = currentGame->creator == netClient->GetPlayerNick();

                                if (selectedActionIndex == 0) {
                                    SubmitLobbyChatInput(netClient);
                                } else if (selectedActionIndex == kRoomSetTeamsTapIndex) {
                                    // ENTER on the header's "Set Teams" button
                                    // opens the picker for host and joiner alike,
                                    // matching the [A] hotkey and the tap path.
                                    // OpenTeamsPanel parks either player on the row
                                    // they may actually change.
                                    OpenTeamsPanel();
                                } else if (selectedActionIndex == kRoomHelpTapIndex) {
                                    // ENTER on the HELP box, now that Up/Down can
                                    // actually reach it (see MenuUpKey/
                                    // MenuDownKey) -- host and joiner alike, same
                                    // as the box's tap target and the F1 hotkey.
                                    showingHelpPanel = true;
                                    helpTopic = (int)HelpTopic::OnlineRoom;
                                    helpScroll = 0;
                                    helpMenuIndex = kHelpRowClose;
                                    PlayMenuSFX("menu_selected");
                                } else if (isHost) {
                                    GameRoomHostReturn(netClient, currentGame);
                                } else if (!isHost && selectedActionIndex == kRoomTeam) {
                                    // Joiner ENTER on the Teams row opens the
                                    // team picker, same as the host's ENTER
                                    // above -- OpenTeamsPanel parks a joiner's
                                    // cursor on their own row whatever column
                                    // was focused, since their own is the only
                                    // one they may change. LEFT/RIGHT here
                                    // still advance it in place as before.
                                    OpenTeamsPanel();
                                }
                                // Non-host has no other actions besides Chat/Teams (use ESC to leave)
                            } else {
                                // In lobby - handle Create/Join actions
                                if (selectedActionIndex == 1) {
                                    // Create new game - do it immediately like original
                                    netClient->CreateGame(kRoomSizes[netRoomSizeChoice]);
                                    netClient->AddStatusMessage("Game created - now you need to wait for players to join");
                                    AudioMixer::Instance()->PlaySFX("menu_selected");
                                } else {
                                    // Join game (selectedActionIndex >= 2)
                                    SDL_Log("Join game action: selectedActionIndex=%d", selectedActionIndex);
                                    std::vector<GameRoom> games = netClient->GetGameList();
                                    // The Discord row sits one past the last
                                    // room, so it would otherwise land in the
                                    // "gameIndex out of bounds" arm below and
                                    // silently do nothing.
                                    if (selectedActionIndex == LobbyDiscordIndex(games.size())) {
                                        OpenDiscordInvite();
                                        AudioMixer::Instance()->PlaySFX("menu_selected");
                                        return;
                                    }
                                    int gameIndex = selectedActionIndex - 2;
                                    SDL_Log("Join game: gameIndex=%d, games.size()=%d", gameIndex, (int)games.size());
                                    if (gameIndex >= 0 && gameIndex < (int)games.size()) {
                                        SDL_Log("Attempting to join game created by: %s", games[gameIndex].creator.c_str());
                                        if (netClient->JoinGame(games[gameIndex].creator.c_str())) {
                                            SDL_Log("JoinGame returned true - successfully joined!");
                                            netClient->AddStatusMessage("Joined game");
                                            AudioMixer::Instance()->PlaySFX("menu_selected");
                                            // Joiner defaults to chat (index 0); can navigate the read-only
                                            // rows and self-select their own Teams column
                                            selectedActionIndex = 0;
                                        } else {
                                            SDL_Log("JoinGame returned false - join failed!");
                                        }
                                    } else {
                                        SDL_Log("Join game: gameIndex out of bounds!");
                                    }
                                }
                            }
                        }
                        return;
                    } else if (showingNetPanel && networkInLobby && networkInputMode == 6) {
                        // Create game confirmed
                        NetworkClient* netClient = NetworkClient::Instance();
                        // Accept CONNECTED or IN_LOBBY — after returning from a game state is IN_LOBBY
                        if (netClient->GetState() == CONNECTED || netClient->GetState() == IN_LOBBY) {
                            if (netClient->CreateGame(kRoomSizes[netRoomSizeChoice])) {
                                AudioMixer::Instance()->PlaySFX("menu_selected");
                            }
                        }
                        networkInputMode = 0; // Back to lobby
                        SDL_StopTextInput(SDL_GetKeyboardFocus());
                        return;
                    } else if (showingNetPanel && networkInLobby && networkInputMode == 5) {
                        // Change username
                        if (strlen(networkUsername) > 0) {
                            NetworkClient* netClient = NetworkClient::Instance();
                            if (netClient->SendNick(networkUsername)) {
                                SDL_Log("Username changed to: %s", networkUsername);
                                AudioMixer::Instance()->PlaySFX("menu_selected");
                            }
                            networkUsername[0] = '\0';
                        }
                        networkInputMode = 0; // Back to lobby
                        SDL_StopTextInput(SDL_GetKeyboardFocus());
                        return;
                    } else if (showingNetPanel && networkInLobby && networkInputMode == 4) {
                        // Send chat message
                        if (strlen(networkChatInput) > 0) {
                            NetworkClient* netClient = NetworkClient::Instance();
                            netClient->SendTalk(networkChatInput);
                            networkChatInput[0] = '\0';
                        }
                        networkInputMode = 0; // Back to lobby
                        SDL_StopTextInput(SDL_GetKeyboardFocus());
                        return;
                    } else if (showingNetPanel && networkInLobby && networkInputMode == 3) {
                        // Join game with entered creator name
                        if (strlen(networkJoinCreator) > 0) {
                            NetworkClient* netClient = NetworkClient::Instance();
                            if (netClient->JoinGame(networkJoinCreator)) {
                                SDL_Log("Joined game created by %s", networkJoinCreator);
                            }
                            networkInputMode = 0; // Back to lobby
                        }
                        return;
                    } else if (showingNetPanel && !networkInLobby && networkInputMode == 11) {
                        // Confirm pre-lobby nickname
                        networkInputMode = networkPreNickReturnMode;
                        SDL_StopTextInput(SDL_GetKeyboardFocus());
                        AudioMixer::Instance()->PlaySFX("menu_selected");
                        SavePreNick();
                        return;
                    } else if (showingNetPanel && !networkInLobby &&
                               (networkInputMode == 8 || networkInputMode == 9)) {
                        if (networkFieldEditing) {
                            // ENTER while keyboard open → close keyboard
                            networkFieldEditing = false;
                            SDL_StopTextInput(SDL_GetKeyboardFocus());
                            AudioMixer::Instance()->PlaySFX("menu_selected");
                            return;
                        } else if (networkManualFieldIndex == 2) {
                            // ENTER on Connect button → connect
                            goto DO_CONNECT;
                        } else {
#ifdef __WASM_PORT__
                            // Touch devices get a native browser prompt instead of the
                            // in-canvas editor (no soft keyboard in SDL3 Emscripten).
                            if (WasmHasTouch()) {
                                if (networkManualFieldIndex == 0) {
                                    char hostBuf[256];
                                    if (WasmPromptText("Server address:", networkHost, hostBuf, sizeof(hostBuf))) {
                                        // Same charset filter as the keyboard path
                                        size_t len = 0;
                                        for (const char* p = hostBuf; *p && len < sizeof(networkHost) - 1; p++) {
                                            char c = *p;
                                            if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
                                                (c >= 'A' && c <= 'Z') || c == '.' || c == '-' || c == ':')
                                                networkHost[len++] = c;
                                        }
                                        networkHost[len] = '\0';
                                        connectErrorMsg.clear();
                                    }
                                } else {
                                    char cur[16], portBuf[16];
                                    snprintf(cur, sizeof(cur), "%d", networkPort);
                                    if (WasmPromptText("Server port:", cur, portBuf, sizeof(portBuf))) {
                                        int p = SDL_atoi(portBuf);
                                        if (p > 0 && p <= 65535) networkPort = p;
                                    }
                                }
                                AudioMixer::Instance()->PlaySFX("menu_selected");
                                return;
                            }
#endif
                            // ENTER on host/port field → open keyboard
                            networkFieldEditing = true;
                            SDL_StopTextInput(SDL_GetKeyboardFocus());
                            SDL_StartTextInput(SDL_GetKeyboardFocus());
                            SetTextInputAreaLogical(const_cast<SDL_Renderer*>(renderer), {160, 152, 320, 20});
                            AudioMixer::Instance()->PlaySFX("menu_selected");
                            return;
                        }
                    } else if (showingNetPanel && !networkInLobby &&
                               (networkInputMode == 7 || networkInputMode == 10)) {
                        DO_CONNECT:
                        // Connect to server
                        const char* host = networkHost;
                        int port = networkPort;
                        if (networkInputMode == 7) {
                            if (lanMenuIndex == 0) {
                                // "Host a server" selected
                                if (!serverHosting) {
                                    StartLocalServer();
                                    // StartLocalServer() only confirms the TCP listener is
                                    // accepting connections; the UDP discovery responder
                                    // that DiscoverLANServers() below broadcasts to isn't
                                    // covered by that readiness poll, so this delay stays
                                    // (unlike the TCP-covered SDL_Delay calls removed
                                    // elsewhere in this file for the async networking
                                    // handoff) -- it just no longer stacks with a second
                                    // blocking discovery scan on top.
                                    SDL_Delay(500);
                                }
                                connectErrorMsg.clear();
                                discoveredServers.clear();
#ifdef __WASM_PORT__
                                discoveredServers = NetworkClient::DiscoverLANServers();
#else
                                StartLanFetch();
#endif
                                lanMenuIndex = 0;
                                return;
                            }
                            int serverIdx = lanMenuIndex - 1;
                            if (serverIdx >= (int)discoveredServers.size()) {
                                // "Set Name" selected (last item)
#ifdef __WASM_PORT__
                                if (WasmHasTouch()) {
                                    // Native prompt on touch devices (no soft keyboard in SDL3 Emscripten)
                                    char nick[32];
                                    if (WasmPromptText("Enter nickname (max 15 chars):", networkPreNick, nick, sizeof(nick))) {
                                        snprintf(networkPreNick, sizeof(networkPreNick), "%.15s", nick);
                                        SavePreNick();
                                    }
                                    AudioMixer::Instance()->PlaySFX("menu_selected");
                                    return;
                                }
#endif
                                networkPreNick[0] = '\0';
                                networkPreNickReturnMode = 7;
                                networkInputMode = 11;
                                SDL_StopTextInput(SDL_GetKeyboardFocus());
                                SDL_StartTextInput(SDL_GetKeyboardFocus());
                                SetTextInputAreaLogical(const_cast<SDL_Renderer*>(renderer), {160, 152, 320, 20});
                                AudioMixer::Instance()->PlaySFX("menu_selected");
                                return;
                            }
                            host = discoveredServers[serverIdx].host.c_str();
                            port = discoveredServers[serverIdx].port;
                        }
                        if (networkInputMode == 10) {
                            if (netMenuIndex == 0) {
                                // "Manual entry" selected — show host/port form without keyboard.
                                // Keyboard opens only when user presses SELECT on host or port field.
                                networkInputMode = 8;
                                networkFieldEditing = false;
                                networkManualFieldIndex = 0;
                                SDL_StopTextInput(SDL_GetKeyboardFocus());
                                return;
                            }
                            // Checked before the Set-name branch below, which is
                            // a catch-all for "any index past the last server"
                            // and would otherwise swallow this one.
                            if (netMenuIndex == ServerListDiscordIndex()) {
                                OpenDiscordInvite();
                                AudioMixer::Instance()->PlaySFX("menu_selected");
                                return;
                            }
                            int serverIdx = netMenuIndex - 1;
                            if (serverIdx >= (int)publicServers.size()) {
                                // "Set Name" selected (last item)
#ifdef __WASM_PORT__
                                if (WasmHasTouch()) {
                                    // Native prompt on touch devices (no soft keyboard in SDL3 Emscripten)
                                    char nick[32];
                                    if (WasmPromptText("Enter nickname (max 15 chars):", networkPreNick, nick, sizeof(nick))) {
                                        snprintf(networkPreNick, sizeof(networkPreNick), "%.15s", nick);
                                        SavePreNick();
                                    }
                                    AudioMixer::Instance()->PlaySFX("menu_selected");
                                    return;
                                }
#endif
                                networkPreNick[0] = '\0';
                                networkPreNickReturnMode = 10;
                                networkInputMode = 11;
                                SDL_StopTextInput(SDL_GetKeyboardFocus());
                                SDL_StartTextInput(SDL_GetKeyboardFocus());
                                SetTextInputAreaLogical(const_cast<SDL_Renderer*>(renderer), {160, 152, 320, 20});
                                AudioMixer::Instance()->PlaySFX("menu_selected");
                                return;
                            }
                            host = publicServers[serverIdx].host.c_str();
                            port = publicServers[serverIdx].port;
                        }
                        NetworkClient* netClient = NetworkClient::Instance(host, port);
                        // Worked out before the branch below, not inside its
                        // first arm: the "still connecting" arm needs the same
                        // nickname to park in networkPreNick for the deferred
                        // lobby entry to pick up.
                        char nickname[32];
                        if (networkPreNick[0] != '\0') {
                            snprintf(nickname, sizeof(nickname), "%s", networkPreNick);
                        } else {
                            const char* envUser = getenv("USER");
                            if (envUser && envUser[0] != '\0') snprintf(nickname, sizeof(nickname), "%s", envUser);
#ifdef __ANDROID__
                            else snprintf(nickname, sizeof(nickname), "android_user");
#else
                            else snprintf(nickname, sizeof(nickname), "unnamed");
#endif
                        }
                        if (netClient->IsConnected()) {
                            AudioMixer::Instance()->PlaySFX("menu_selected");
                            if (netClient->SendNick(nickname)) {
                                // Only save if nick was explicitly set (not auto-filled from env)
                                if (networkPreNick[0] != '\0') {
#ifdef __WASM_PORT__
                                    EM_ASM({ localStorage.setItem('fb_nickname', UTF8ToString($0)); }, nickname);
#else
                                    GameSettings* gsn = GameSettings::Instance();
                                    snprintf(gsn->savedNickname, sizeof(gsn->savedNickname), "%s", nickname);
                                    gsn->SaveKeys();
#endif
                                }
                                // Geoloc fetch runs on a background thread and is sent
                                // whenever it finishes (see PollGeoLocFetch, driven from
                                // PumpNetworkFrame), independent of lobby entry -- see the
                                // matching comment in NetPanelRender's pendingLobbyConnect
                                // completion for why this is safe to decouple.
                                StartGeoLocFetch();
                                networkInLobby = true;
                                networkInputMode = 0;  // Switch to lobby mode so C/J/T/U keys work
                                networkGameStarting = false;
                                netStartRequested = false;
                                syncWaitStart = 0;
                                wasmBotWaitStart = 0;
                                netClient->RequestList();  // Immediate list on lobby entry
                                lastListRequest = SDL_GetTicks();
#ifdef __ANDROID__
                                SDL_SendAndroidMessage(0x8001, 0); // show lobby ad
#endif
                            } else {
                                // SendNick failed — WebSocket is still connecting (WASM async).
                                // Store the nickname and complete lobby entry in NetPanelRender()
                                // once the WebSocket open callback fires and state becomes CONNECTED.
                                SDL_Log("SendNick failed (state=%d), setting pendingLobbyConnect for async completion", netClient->GetState());
                                snprintf(networkPreNick, sizeof(networkPreNick), "%s", nickname);
                                pendingLobbyConnect = true;
                                connectErrorMsg.clear();
                            }
                        } else if (netClient->IsConnecting()) {
                            // Connection under way but not finished: name lookup,
                            // TCP connect, or the SERVER_READY handshake is still
                            // outstanding (native, async networking handoff stage
                            // 2a-2c), or the WebSocket has not opened yet (WASM,
                            // where this has always been the normal path). Park
                            // the nickname and let NetPanelRender()'s
                            // pendingLobbyConnect block finish the job once the
                            // state reaches CONNECTED.
                            //
                            // This arm is not optional. Without it a still-
                            // connecting client falls into the "failed" arm below
                            // and is told the server is unreachable while the
                            // connection is still perfectly alive.
                            SDL_Log("Connect in progress (state=%d), setting pendingLobbyConnect",
                                    netClient->GetState());
                            snprintf(networkPreNick, sizeof(networkPreNick), "%s", nickname);
                            pendingLobbyConnect = true;
                            connectErrorMsg.clear();
                        } else {
                            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to connect to %s:%d", host, port);
                            char errBuf[320];
                            snprintf(errBuf, sizeof(errBuf), "Failed to connect to %s:%d", host, port);
                            connectErrorMsg = errBuf;
                        }
                    } else {
                        press();
                    }
                    return;
}

void MainMenu::MenuEscapeKey() {
                    if (showingSPPanel) {
                        AudioMixer::Instance()->PlaySFX("cancel");
                        showingSPPanel = false;
                        return;
                    }
                    if (showingNetPanel) {
                        AudioMixer::Instance()->PlaySFX("cancel");
                        if (networkInputMode == 11) {
                            // Cancel pre-lobby nickname editing
                            networkInputMode = networkPreNickReturnMode;
                            SDL_StopTextInput(SDL_GetKeyboardFocus());
                            return;
                        } else if (networkInputMode == 8 || networkInputMode == 9) {
                            if (networkFieldEditing) {
                                // ESC while keyboard is open: close keyboard, stay on form
                                networkFieldEditing = false;
                                SDL_StopTextInput(SDL_GetKeyboardFocus());
                            } else {
                                // ESC with no keyboard: back to public server list
                                networkInputMode = 10;
                            }
                            return;
                        } else if (networkInputMode == 10) {
                            // Close net panel. Cancel first: a connect started
                            // from this list can still be in flight (async
                            // networking handoff, stage 2a-2c), and leaving it
                            // running would complete into a lobby the player
                            // has already left. This branch used to neither
                            // disconnect nor clear pendingLobbyConnect.
                            CancelPendingConnect();
                            showingNetPanel = false;
                            return;
                        } else if (networkInputMode == 6) {
                            // Cancel create game confirmation
                            networkInputMode = 0;
                            return;
                        } else if (networkInputMode == 5) {
                            // Cancel username input
                            networkInputMode = 0;
                            networkUsername[0] = '\0';
                            SDL_StopTextInput(SDL_GetKeyboardFocus());
                            return;
                        } else if (networkInputMode == 4) {
                            // Cancel chat input
                            networkInputMode = 0;
                            networkChatInput[0] = '\0';
                            return;
                        } else if (networkInputMode == 3) {
                            // Cancel join input
                            networkInputMode = 0;
                            networkJoinCreator[0] = '\0';
                            return;
                        } else if (networkInputMode == 0 && networkInLobby) {
                            // In lobby - check if in a game
                            NetworkClient* netClient = NetworkClient::Instance();
                            GameRoom* currentGame = netClient->GetCurrentGame();
                            if (currentGame) {
                                // Leave the game (like original)
                                showingTeamsPanel = false;
                                netStartRequested = false;  // don't carry a pending Start into the next room
                                DropLobbyBots();  // our bots leave with us
                                netClient->PartGame();
                                netClient->RequestList();  // Immediate list after parting
                                lastListRequest = SDL_GetTicks();
                                netClient->AddStatusMessage("*** Leaving game...");
                            } else {
                                // Not in a game -- disconnect from this server, but
                                // land back on the server list (mode 10) rather than
                                // closing the whole net panel out to the main menu:
                                // backing out of "this server's lobby" means "let me
                                // pick a different server", not "leave NET GAME
                                // entirely". Mirrors mode 8/9's own ESC handling
                                // above, which already treats mode 10 as the strictly
                                // less-committed screen to fall back to.
                                networkInLobby = false;
                                networkInputMode = 10;
                                if (netClient->IsConnected()) {
                                    netClient->Disconnect();
                                }
                                // Same refresh the list's own 'R' key does (below),
                                // not mode 10's full from-main-menu setup (mainmenu.cpp
                                // case 5) -- this is "show me other servers", not
                                // "reset every hosting option back to its default".
                                connectErrorMsg.clear();
                                netMenuIndex = 0;
#ifdef __WASM_PORT__
                                publicServers = NetworkClient::FetchPublicServers();
#else
                                publicServers.clear();
                                StartPublicServerFetch();
#endif
                            }
                        } else {
                            showingNetPanel = false;
                            networkInLobby = false;
                            // Two different things to tear down, and the
                            // IsConnected() check alone catches only one of
                            // them: an attempt still in flight is not
                            // "connected" (see IsConnected()'s definition), so
                            // without this a LAN connect started seconds ago
                            // would keep running after its screen was gone.
                            CancelPendingConnect();
                            if (NetworkClient::Instance()->IsConnected()) {
                                NetworkClient::Instance()->Disconnect();
                            }
                            if (serverHosting) {
                                StopLocalServer();
                            }
                        }
                        return;
                    }
                    if (showingLevelPanel) {
                        AudioMixer::Instance()->PlaySFX("cancel");
                        showingLevelPanel = false;
                        SDL_StopTextInput(SDL_GetKeyboardFocus());
                        runDelay = false;
                        return;
                    }
                    if (showingKeysPanel) {
                        AudioMixer::Instance()->PlaySFX("cancel");
                        showingKeysPanel = false;
                        resetAllArmed = false;
                        GameSettings::Instance()->SaveKeys();
                        return;
                    }
                    if (showingLocalMPPanel) {
                        AudioMixer::Instance()->PlaySFX("cancel");
                        showingLocalMPPanel = false;
                        return;
                    }
                    if (showingOptPanel || showingNetSetupPanel) {
                        AudioMixer::Instance()->PlaySFX("cancel");
                        showingOptPanel = showingNetSetupPanel = false;
                        awaitKp = false;
                        return;
                    }
                    // On Android TV, pressing Back at the root menu should not
                    // quit the game — the Fire TV launcher injects KEYCODE_BACK
                    // during the launch transition, which would immediately kill
                    // the app. Users navigate away via the Home button instead.
#ifndef __ANDROID__
                    FrozenBubble::Instance()->CallGameQuit();
#endif
                    return;
}
