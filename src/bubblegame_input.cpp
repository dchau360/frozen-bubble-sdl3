/*
 * Frozen-Bubble SDL2 C++ Port
 * Copyright (c) 2000-2012 The Frozen-Bubble Team
 * Copyright (c) 2026 Huy Chau
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

#include "frozenbubble.h"
#include "bubblegame.h"
#include "audiomixer.h"
#include "highscoremanager.h"
#include "transitionmanager.h"
#include "gamesettings.h"
#include "platform.h"

#include <fstream>
#include <sstream>
#include <set>
#include <queue>
#include <map>

#include <cmath>
#include <algorithm>
#include "bubblegame_internal.h"

bool BubbleGame::HandleFinishedTap(float lx, float ly) {
    if (!currentSettings.networkGame || !gameFinish) return false;
    if (statsChatBtn.w <= 0) return false;
    if (lx < statsChatBtn.x || lx >= statsChatBtn.x + statsChatBtn.w ||
        ly < statsChatBtn.y || ly >= statsChatBtn.y + statsChatBtn.h)
        return false;
    if (chattingMode) FinishInGameChat(true);
    else StartInGameChat();
    return true;
}

void BubbleGame::HandleMouseAim(float mx, float my) {
    if (chattingMode) return;
    if (!currentSettings.mouseEnabled) return;
    if (currentSettings.playerCount < 1) return;
    BubbleArray& bArr = bubbleArrays[0];
    const SDL_Rect& r = bArr.shooterSprite.rect;
    float sx = r.x + r.w * 0.5f;
    float sy = r.y + r.h * 0.5f;
    float dx = mx - sx;
    float dy = sy - my;  // flip y: up = positive
    // Only aim when mouse is above the shooter barrel (prevent backward shots)
    if (my > sy - 5.f) return;
    float ang = atan2f(dy, dx);
    if (ang < 0.1f) ang = 0.1f;
    if (ang > (float)PI - 0.1f) ang = (float)PI - 0.1f;
    bArr.mouseTargetAngle = ang;
}

void BubbleGame::HandleMouseFire() {
    if (chattingMode) return;
    if (!currentSettings.mouseEnabled) return;
    if (currentSettings.playerCount < 1) return;
    bubbleArrays[0].mouseFirePending = true;
}

void BubbleGame::StartInGameChat() {
    if (!currentSettings.networkGame || chattingMode) return;
#ifdef __WASM_PORT__
    // SDL3's Emscripten backend cannot summon the mobile soft keyboard, so on
    // touch devices compose the message in a native browser prompt and send it
    // immediately instead of opening the in-canvas editor.
    if (WasmHasTouch()) {
        char msg[256] = "";
        if (WasmPromptText("Chat message:", "", msg, sizeof(msg)) && msg[0] != '\0') {
            chattingMode = true;
            snprintf(chatInputBuf, sizeof(chatInputBuf), "%s", msg);
            FinishInGameChat(true);
        }
        return;
    }
#endif
    chattingMode = true;
    chatInputBuf[0] = '\0';
    SDL_StartTextInput(SDL_GetKeyboardFocus());
    SDL_Log("In-game chat opened (betweenRounds=%d)", gameFinish);
}

void BubbleGame::FinishInGameChat(bool sendMessage) {
    if (!chattingMode) return;

    if (sendMessage && chatInputBuf[0] != '\0') {
        NetworkClient* netClient = NetworkClient::Instance();
        if (netClient && netClient->IsConnected() && netClient->GetState() == IN_GAME) {
            char talkMsg[258];
            snprintf(talkMsg, sizeof(talkMsg), "t%s", chatInputBuf);
            netClient->SendGameData(talkMsg);
            SDL_Log("In-game chat sent (betweenRounds=%d, length=%zu)",
                    gameFinish, strlen(chatInputBuf));

            InGameChatMsg self;
            self.nick = netClient->GetPlayerNick();
            if (self.nick.empty()) self.nick = "Me";
            self.text = chatInputBuf;
            self.framesLeft = 600;
            inGameChatMessages.push_back(self);
            if (inGameChatMessages.size() > 10)
                inGameChatMessages.erase(inGameChatMessages.begin());
        }
    }

    chatInputBuf[0] = '\0';
    chattingMode = false;
    SDL_StopTextInput(SDL_GetKeyboardFocus());
    SDL_Log("In-game chat closed (sent=%d)", sendMessage);
}

void BubbleGame::HandleInput(SDL_Event *e) {
    // Map gamepad/D-pad to keyboard-equivalent actions
    if (e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        if (currentSettings.localMultiplayer) {
            // In local multiplayer, per-player movement is polled directly in UpdatePenguin.
            // Only handle global buttons (B=quit, START=pause) via fake key events.
            // A button also handled when game is finished so players can continue to next round.
            SDL_KeyboardEvent fake{};
            fake.type = SDL_EVENT_KEY_DOWN;
            fake.down = true;
            switch (e->gbutton.button) {
                case SDL_GAMEPAD_BUTTON_EAST:    fake.key = SDLK_ESCAPE; break;
                case SDL_GAMEPAD_BUTTON_START:   fake.key = SDLK_P; break;
                case SDL_GAMEPAD_BUTTON_SOUTH:
                    if (gameFinish) { fake.key = SDLK_SPACE; break; }
                    return;
                default: return;
            }
            SDL_Event fakeEvent;
            fakeEvent.type = SDL_EVENT_KEY_DOWN;
            fakeEvent.key = fake;
            HandleInput(&fakeEvent);
            return;
        }
        SDL_KeyboardEvent fake{};
        fake.type = SDL_EVENT_KEY_DOWN;
        fake.down = true;
        switch (e->gbutton.button) {
            case SDL_GAMEPAD_BUTTON_DPAD_LEFT:  fake.key = SDLK_LEFT; break;
            case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: fake.key = SDLK_RIGHT; break;
            case SDL_GAMEPAD_BUTTON_SOUTH:      fake.key = SDLK_SPACE; break;
            case SDL_GAMEPAD_BUTTON_EAST:       fake.key = SDLK_ESCAPE; break;
            case SDL_GAMEPAD_BUTTON_START:      fake.key = SDLK_P; break;
            case SDL_GAMEPAD_BUTTON_WEST:       fake.key = SDLK_T; break; // X=Chat
            default: return;
        }
        SDL_Event fakeEvent;
        fakeEvent.type = SDL_EVENT_KEY_DOWN;
        fakeEvent.key = fake;
        HandleInput(&fakeEvent);
        return;
    }

    switch(e->type) {
        case SDL_EVENT_TEXT_INPUT:
            if (chattingMode) {
                size_t curLen = strlen(chatInputBuf);
                size_t addLen = strlen(e->text.text);
                if (curLen + addLen < sizeof(chatInputBuf) - 1)
                    strcat(chatInputBuf, e->text.text);
            }
            return;
        case SDL_EVENT_KEY_DOWN:
            // The text editor owns every key while open. Characters arrive through
            // SDL_EVENT_TEXT_INPUT; consuming key-down events prevents gameplay actions.
            if (chattingMode) {
                if (e->key.key == SDLK_BACKSPACE) {
                    size_t len = strlen(chatInputBuf);
                    if (len > 0) chatInputBuf[len - 1] = '\0';
                } else if (!e->key.repeat &&
                           (e->key.key == SDLK_RETURN || e->key.key == SDLK_KP_ENTER)) {
                    FinishInGameChat(true);
                } else if (!e->key.repeat &&
                           (e->key.key == SDLK_ESCAPE || e->key.key == SDLK_AC_BACK)) {
                    FinishInGameChat(false);
                }
                return;
            }
            if(e->key.repeat) break;
            switch(e->key.key) {
                case SDLK_AC_BACK:
                case SDLK_ESCAPE:
                    QuitToTitle();
                    break;
                case SDLK_F11: // mute / unpause audio
                    if(audMixer->IsHalted() == true) {
                        audMixer->MuteAll(true);
                        audMixer->PlayMusic("main1p");
                    }
                    else audMixer->MuteAll();
                    break;
                case SDLK_C: // toggle colorblind mode
                    {
                        GameSettings *settings = GameSettings::Instance();
                        bool currentMode = settings->colorBlind();
                        settings->SetValue("GFX:ColorblindBubbles", currentMode ? "false" : "true");
                        SDL_Log("Colorblind mode: %s", currentMode ? "OFF" : "ON");
                    }
                    break;
                case SDLK_T:
                    if (currentSettings.networkGame) StartInGameChat();
                    break;
                // Single player targeting keys (original lines 1681-1690)
                // Keys 1-4 target opponents rp1-rp4, key 0 or 5 clears targeting
                case SDLK_1:
                case SDLK_2:
                case SDLK_3:
                case SDLK_4:
                case SDLK_0:
                    if (currentSettings.networkGame && currentSettings.playerCount >= 3 &&
                        currentSettings.singlePlayerTargetting && !gameFinish) {
                        int target = -1;
                        if (e->key.key == SDLK_1) target = 1;
                        else if (e->key.key == SDLK_2 && currentSettings.playerCount >= 3) target = 2;
                        else if (e->key.key == SDLK_3 && currentSettings.playerCount >= 4) target = 3;
                        else if (e->key.key == SDLK_4 && currentSettings.playerCount >= 5) target = 4;
                        // SDLK_0 stays -1 to clear targeting
                        SetSendMalusToOne(target);
                    }
                    break;
                case SDLK_SPACE:
                    // Fire TV remote A button (mapped to SDLK_SPACE) should also continue
                    // the round when the game is finished — same as SDLK_RETURN below.
                    // During active play SDLK_SPACE is the fire key and is handled elsewhere,
                    // so only fall through here when gameFinish is set.
                    if (gameFinish && singleBubbles.size() == 0)
                        goto handle_return;
                    break;
                default:
                    // Any configured fire key (up arrow, C, V, etc.) continues round when finished
                    if (gameFinish && singleBubbles.size() == 0) {
                        GameSettings* gs2 = GameSettings::Instance();
                        SDL_Scancode sc = e->key.scancode;
                        if (sc == gs2->player1Keys.fire || sc == gs2->player2Keys.fire ||
                            sc == gs2->player3Keys.fire || sc == gs2->player4Keys.fire ||
                            sc == gs2->player5Keys.fire)
                            goto handle_return;
                    }
                    break;
                case SDLK_RETURN:
                handle_return:
                    // During play, RETURN opens chat. Between rounds it retains its
                    // next-round action; T or gamepad X opens chat in either state.
                    if (currentSettings.networkGame && !gameFinish) {
                        StartInGameChat();
                        break;
                    }
                    if (!gameFinish || (gameFinish && singleBubbles.size() > 0)) break;

                    // In network game, synchronize new game with opponent
                    if (currentSettings.networkGame) {
                        // If match is over (victories limit reached), return to lobby
                        if (gameMatchOver) {
                            SDL_Log("Match over - victories limit reached, returning to lobby");
                            QuitToTitle();
                            break;
                        }
                        // If all opponents have disconnected, no one to play with - return to lobby
                        if (connectedPlayerCount <= 1) {
                            SDL_Log("No connected opponents remain - returning to lobby");
                            QuitToTitle();
                            break;
                        }
                        // Send 'n' and start waiting - render loop will start game when both ready
                        // Guard: don't send 'n' again if already waiting (prevents double-counting at peers)
                        if (!waitingForOpponentNewGame) {
                            SDL_Log("Sending newgame signal 'n' to opponent (gameFinish=%d, gameWon=%d, gameLost=%d)",
                                    gameFinish, gameWon, gameLost);
                            NetworkClient* netClient = NetworkClient::Instance();
                            if (netClient->IsConnected() && netClient->GetState() == IN_GAME) {
                                netClient->SendGameData("n");
                                waitingForOpponentNewGame = true;
                                // Don't reset opponentsReadyCount here — 'n' messages from peers may have
                                // already arrived before we pressed ENTER, and resetting would lose them.
                                // The count is reset in the render loop when the new game actually starts.
                                SDL_Log("Waiting for all opponents to be ready (already have %d/%d)...",
                                        opponentsReadyCount, connectedPlayerCount - 1);
                            }
                        }
                    } else {
                        // Single player or local multiplayer
                        if (currentSettings.mpTraining && mpTrainDone) {
                            // Training ended: show highscores or return to title
                            if (pendingHighscore) {
                                pendingHighscore = false;
                                HighscoreManager::Instance()->ShowNewScorePanel(0);
                                HighscoreManager::Instance()->ShowScoreScreen(0);
                            } else {
                                QuitToTitle();
                            }
                        } else if (gameWon || gameMpDone) {
                            ++curLevel;
                            // If a highscore was earned and this would end the game, show score screen first
                            bool willEnd = (curLevel >= (int)loadedLevels.size() && !currentSettings.randomLevels);
                            if (willEnd && pendingHighscore) {
                                pendingHighscore = false;
                                HighscoreManager::Instance()->ShowNewScorePanel(0);
                                HighscoreManager::Instance()->ShowScoreScreen(0);
                            } else {
                                ReloadGame(curLevel);
                            }
                        } else if (gameLost) {
                            ReloadGame(curLevel);
                        }
                    }
                    break;
            }
            break;
    }
}
