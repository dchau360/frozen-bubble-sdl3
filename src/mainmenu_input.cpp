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

#include "mainmenu.h"
#include "audiomixer.h"
#include "frozenbubble.h"
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

void MainMenu::HandleInput(SDL_Event *e){
    // Map gamepad/D-pad to keyboard-equivalent actions for TV remotes
    if (e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        SDL_KeyboardEvent fake{};
        fake.type = SDL_EVENT_KEY_DOWN;
        fake.down = true;
        switch (e->gbutton.button) {
            case SDL_GAMEPAD_BUTTON_DPAD_UP:    fake.key = SDLK_UP; break;
            case SDL_GAMEPAD_BUTTON_DPAD_DOWN:  fake.key = SDLK_DOWN; break;
            case SDL_GAMEPAD_BUTTON_DPAD_LEFT:  fake.key = SDLK_LEFT; break;
            case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: fake.key = SDLK_RIGHT; break;
            case SDL_GAMEPAD_BUTTON_SOUTH:          fake.key = SDLK_RETURN; break;
            case SDL_GAMEPAD_BUTTON_EAST:          fake.key = SDLK_ESCAPE; break;
            case SDL_GAMEPAD_BUTTON_START:      fake.key = SDLK_RETURN; break;
            case SDL_GAMEPAD_BUTTON_WEST:          fake.key = SDLK_T; break; // X=Chat
            case SDL_GAMEPAD_BUTTON_NORTH:          fake.key = SDLK_R; break; // Y=Remove Ads (Android opts panel)
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
                size_t len = strlen(networkPreNick);
                for (const char* p = e->text.text; *p; p++) {
                    char c = *p;
                    if (c == '\b') {
                        if (len > 0) { networkPreNick[--len] = '\0'; }
                    } else if (len < 15) {
                        networkPreNick[len++] = c;
                        networkPreNick[len] = '\0';
                    }
                }
            }
            // Handle virtual keyboard character input for chat (mode 4)
            if (showingNetPanel && networkInLobby && networkInputMode == 4) {
                size_t len = strlen(networkChatInput);
                for (const char* p = e->text.text; *p; p++) {
                    char c = *p;
                    if (c == '\b') {
                        if (len > 0) { networkChatInput[--len] = '\0'; }
                    } else if (len < 255) {
                        networkChatInput[len++] = c;
                        networkChatInput[len] = '\0';
                    }
                }
            }
            // Handle virtual keyboard character input for username (mode 5)
            if (showingNetPanel && networkInLobby && networkInputMode == 5) {
                size_t len = strlen(networkUsername);
                for (const char* p = e->text.text; *p; p++) {
                    char c = *p;
                    if (c == '\b') {
                        if (len > 0) { networkUsername[--len] = '\0'; }
                    } else if (len < 31) {
                        networkUsername[len++] = c;
                        networkUsername[len] = '\0';
                    }
                }
            }
            break;
        case SDL_EVENT_KEY_DOWN:
            // Handle backspace/delete in text fields before the repeat filter,
            // because Android's IME may send backspace with repeat=1 on a single press.
            if (e->key.key == SDLK_BACKSPACE || e->key.key == SDLK_DELETE) {
                if (showingNetPanel && !networkInLobby && networkInputMode == 8 && networkFieldEditing) {
                    size_t len = strlen(networkHost);
                    if (len > 0) networkHost[len - 1] = '\0';
                    break;
                }
                if (showingNetPanel && !networkInLobby && networkInputMode == 11) {
                    size_t len = strlen(networkPreNick);
                    if (len > 0) networkPreNick[len - 1] = '\0';
                    break;
                }
            }
            if(e->key.repeat) break;

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
                    break;
                } else if (e->key.key == SDLK_BACKSPACE) {
                    size_t len = strlen(networkJoinCreator);
                    if (len > 0) networkJoinCreator[len - 1] = '\0';
                    break;
                }
            } else if (showingNetPanel && networkInLobby && networkInputMode == 4) {
                // Chat input - characters handled by SDL_EVENT_TEXT_INPUT
                if (e->key.key == SDLK_BACKSPACE) {
                    size_t len = strlen(networkChatInput);
                    if (len > 0) networkChatInput[len - 1] = '\0';
                    break;
                }
            } else if (showingNetPanel && networkInLobby && networkInputMode == 5) {
                // Username input - characters handled by SDL_EVENT_TEXT_INPUT
                if (e->key.key == SDLK_BACKSPACE) {
                    size_t len = strlen(networkUsername);
                    if (len > 0) networkUsername[len - 1] = '\0';
                    break;
                }
            } else if (showingNetPanel && !networkInLobby && networkInputMode == 11) {
                // Pre-lobby nickname input - characters handled by SDL_EVENT_TEXT_INPUT
                if (e->key.key == SDLK_BACKSPACE) {
                    size_t len = strlen(networkPreNick);
                    if (len > 0) networkPreNick[len - 1] = '\0';
                    break;
                }
            }

            // Handle backspace in level panel
            if (showingLevelPanel && !runDelay) {
                if (e->key.key == SDLK_BACKSPACE) {
                    if (!levelInput.empty()) levelInput.pop_back();
                    break;
                }
            }

            // Handle backspace in chat input when Chat is selected in lobby
            if (showingNetPanel && networkInLobby && networkInputMode == 0 && selectedActionIndex == 0) {
                if (e->key.key == SDLK_BACKSPACE) {
                    size_t len = strlen(networkChatInput);
                    if (len > 0) networkChatInput[len - 1] = '\0';
                    break;
                }
            }

            // Digit input for level selection panel
            if (showingLevelPanel && !runDelay) {
                if (e->key.key >= SDLK_0 && e->key.key <= SDLK_9) {
                    if (levelInput.size() < 3) {
                        levelInput += (char)('0' + (e->key.key - SDLK_0));
                    }
                    break;
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
                            break;
                        }
                    } else if (networkManualFieldIndex == 1) {
                        // Port field editing
                        if (e->key.key >= SDLK_0 && e->key.key <= SDLK_9) {
                            if (networkPort < 6553) networkPort = networkPort * 10 + (e->key.key - SDLK_0);
                            break;
                        } else if (e->key.key == SDLK_BACKSPACE) {
                            networkPort /= 10;
                            break;
                        }
                    }
                } else {
                    // Not editing: UP/DOWN cycles through host, port, connect
                    if (e->key.key == SDLK_DOWN) {
                        networkManualFieldIndex = (networkManualFieldIndex + 1) % 3;
                        networkInputMode = (networkManualFieldIndex == 1) ? 9 : 8;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                        break;
                    } else if (e->key.key == SDLK_UP) {
                        networkManualFieldIndex = (networkManualFieldIndex + 2) % 3;
                        networkInputMode = (networkManualFieldIndex == 1) ? 9 : 8;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                        break;
                    }
                }
            }

#ifdef __ANDROID__
            // On Android: pressing 'R' in any options panel triggers "Remove Ads" IAP
            if (awaitKp && (showingOptPanel || showingNetSetupPanel) &&
                e->key.key == SDLK_R) {
                SDL_SendAndroidMessage(0x8002, 0); // launch Remove Ads purchase flow
                break;
            }
#endif

            if (awaitKp && (showingOptPanel || showingNetSetupPanel) && e->key.key != SDLK_ESCAPE) {
                AudioMixer::Instance()->PlaySFX("typewriter");
                lastOptInput = e->key.key;
                awaitKp = false;
                break;
            }

            // Keys configuration input handling
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
                    break;
                } else if (!awaitKp) {
                    // UP/DOWN: navigate keys within current player
                    if (e->key.key == SDLK_UP) {
#ifdef __WASM_PORT__
                        keyConfigIndex = (keyConfigIndex == 0) ? 7 : keyConfigIndex - 1;
#else
                        keyConfigIndex = (keyConfigIndex == 0) ? 8 : keyConfigIndex - 1;
#endif
                        AudioMixer::Instance()->PlaySFX("menu_change");
                        break;
                    } else if (e->key.key == SDLK_DOWN) {
#ifdef __WASM_PORT__
                        keyConfigIndex = (keyConfigIndex == 7) ? 0 : keyConfigIndex + 1;
#else
                        keyConfigIndex = (keyConfigIndex == 8) ? 0 : keyConfigIndex + 1;
#endif
                        AudioMixer::Instance()->PlaySFX("menu_change");
                        break;
                    } else if (e->key.key == SDLK_LEFT) {
                        if (keyConfigIndex == 5) {
                            // Decrease game speed
                            GameSettings* gs = GameSettings::Instance();
                            gs->speedMultiplier -= 0.1f;
                            if (gs->speedMultiplier < 1.0f) gs->speedMultiplier = 1.0f;
                            gs->SaveKeys();
                            AudioMixer::Instance()->PlaySFX("menu_change");
                            break;
                        }
                        // Previous player
                        keyConfigPlayer = (keyConfigPlayer == 1) ? 4 : keyConfigPlayer - 1;
                        keyConfigIndex = 0;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                        break;
                    } else if (e->key.key == SDLK_RIGHT) {
                        if (keyConfigIndex == 5) {
                            // Increase game speed
                            GameSettings* gs = GameSettings::Instance();
                            gs->speedMultiplier += 0.1f;
                            if (gs->speedMultiplier > 5.0f) gs->speedMultiplier = 5.0f;
                            gs->SaveKeys();
                            AudioMixer::Instance()->PlaySFX("menu_change");
                            break;
                        }
                        // Next player
                        keyConfigPlayer = (keyConfigPlayer == 4) ? 1 : keyConfigPlayer + 1;
                        keyConfigIndex = 0;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                        break;
                    } else if (e->key.key == SDLK_RETURN) {
                        if (keyConfigIndex == 4) {
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
                        } else if (keyConfigIndex == 5) {
                            // Game Speed row — adjusted via LEFT/RIGHT, ENTER does nothing
                        } else if (keyConfigIndex == 6) {
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
                        } else if (keyConfigIndex == 7) {
                            // Toggle mouse/touch aim
                            GameSettings* gs = GameSettings::Instance();
                            gs->mouseEnabled = !gs->mouseEnabled;
                            gs->SaveKeys();
                            AudioMixer::Instance()->PlaySFX("menu_change");
#ifndef __WASM_PORT__
                        } else if (keyConfigIndex == 8) {
                            // Toggle fullscreen
                            GameSettings* gs = GameSettings::Instance();
                            gs->SetValue("GFX:Fullscreen", "");
                            SDL_SetWindowFullscreen(SDL_GetRenderWindow(const_cast<SDL_Renderer*>(renderer)), gs->fullscreenMode());
                            AudioMixer::Instance()->PlaySFX("menu_change");
#endif
                        } else {
                            // Wait for key press
                            AudioMixer::Instance()->PlaySFX("menu_selected");
                            awaitKp = true;
                        }
                        break;
                    }
                }
            }

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
                    break; // Don't process this key further
                }
            }

            // Handle local multiplayer setup panel navigation
            if (showingLocalMPPanel && !runDelay) {
                // 0=Players, 1=CR, 2=Row collapse, 3..3+N-1=Aim guide per player, 3+N..3+2N-1=Colors per player, 3+2N=Start
                int localMaxIdx = 3 + 2 * localMPPlayerCount;
                if (e->key.key == SDLK_UP) {
                    localMPMenuIndex--;
                    if (localMPMenuIndex < 0) localMPMenuIndex = localMaxIdx;
                    AudioMixer::Instance()->PlaySFX("menu_change");
                    break;
                } else if (e->key.key == SDLK_DOWN) {
                    localMPMenuIndex++;
                    if (localMPMenuIndex > localMaxIdx) localMPMenuIndex = 0;
                    AudioMixer::Instance()->PlaySFX("menu_change");
                    break;
                } else if (e->key.key == SDLK_LEFT || e->key.key == SDLK_RIGHT) {
                    if (localMPMenuIndex == 0) {
                        if (e->key.key == SDLK_LEFT) {
                            localMPPlayerCount--;
                            if (localMPPlayerCount < 2) localMPPlayerCount = 4;
                        } else {
                            localMPPlayerCount++;
                            if (localMPPlayerCount > 4) localMPPlayerCount = 2;
                        }
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex == 1) {
                        localMPCR = !localMPCR;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex == 2) {
                        localMPNoCompress = !localMPNoCompress;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex >= 3 && localMPMenuIndex < 3 + localMPPlayerCount) {
                        int pi = localMPMenuIndex - 3;
                        localMPAimGuide[pi] = !localMPAimGuide[pi];
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex >= 3 + localMPPlayerCount && localMPMenuIndex < 3 + 2 * localMPPlayerCount) {
                        int pi = localMPMenuIndex - 3 - localMPPlayerCount;
                        if (e->key.key == SDLK_LEFT) {
                            playerColorCounts[pi]--;
                            if (playerColorCounts[pi] < 5) playerColorCounts[pi] = 8;
                        } else {
                            playerColorCounts[pi]++;
                            if (playerColorCounts[pi] > 8) playerColorCounts[pi] = 5;
                        }
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    }
                    break;
                } else if (e->key.key == SDLK_RETURN) {
                    if (localMPMenuIndex == 0) {
                        localMPPlayerCount++;
                        if (localMPPlayerCount > 4) localMPPlayerCount = 2;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex == 1) {
                        localMPCR = !localMPCR;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex == 2) {
                        localMPNoCompress = !localMPNoCompress;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex >= 3 && localMPMenuIndex < 3 + localMPPlayerCount) {
                        int pi = localMPMenuIndex - 3;
                        localMPAimGuide[pi] = !localMPAimGuide[pi];
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex >= 3 + localMPPlayerCount && localMPMenuIndex < 3 + 2 * localMPPlayerCount) {
                        int pi = localMPMenuIndex - 3 - localMPPlayerCount;
                        playerColorCounts[pi]++;
                        if (playerColorCounts[pi] > 8) playerColorCounts[pi] = 5;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex == localMaxIdx) {
                        // Start game!
                        chainReaction = localMPCR;
                        AudioMixer::Instance()->PlaySFX("menu_selected");
                        delayTime = 60;
                        runDelay = true;
                    }
                    break;
                } else if (e->key.key == SDLK_ESCAPE) {
                    showingLocalMPPanel = false;
                    AudioMixer::Instance()->PlaySFX("menu_change");
                    break;
                }
            }

            // Handle 2-player game setup menu navigation
            if (showing2PPanel && !awaitKp) {
                // 0=CR, 1=Victories, 2=Colors P1, 3=Colors P2, 4=Start
                if (e->key.key == SDLK_UP) {
                    twoPlayerMenuIndex--;
                    if (twoPlayerMenuIndex < 0) twoPlayerMenuIndex = 4;
                    AudioMixer::Instance()->PlaySFX("menu_change");
                    break;
                } else if (e->key.key == SDLK_DOWN) {
                    twoPlayerMenuIndex++;
                    if (twoPlayerMenuIndex > 4) twoPlayerMenuIndex = 0;
                    AudioMixer::Instance()->PlaySFX("menu_change");
                    break;
                } else if (e->key.key == SDLK_LEFT || e->key.key == SDLK_RIGHT) {
                    if (twoPlayerMenuIndex == 0) {
                        twoPlayerCR = !twoPlayerCR;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (twoPlayerMenuIndex == 1) {
                        if (e->key.key == SDLK_LEFT) {
                            twoPlayerVictoriesIndex--;
                            if (twoPlayerVictoriesIndex < 0) twoPlayerVictoriesIndex = 17;
                        } else {
                            twoPlayerVictoriesIndex++;
                            if (twoPlayerVictoriesIndex > 17) twoPlayerVictoriesIndex = 0;
                        }
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (twoPlayerMenuIndex == 2 || twoPlayerMenuIndex == 3) {
                        int pi = twoPlayerMenuIndex - 2;
                        if (e->key.key == SDLK_LEFT) {
                            playerColorCounts[pi]--;
                            if (playerColorCounts[pi] < 5) playerColorCounts[pi] = 8;
                        } else {
                            playerColorCounts[pi]++;
                            if (playerColorCounts[pi] > 8) playerColorCounts[pi] = 5;
                        }
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    }
                    break;
                } else if (e->key.key == SDLK_RETURN) {
                    if (twoPlayerMenuIndex == 0) {
                        twoPlayerCR = !twoPlayerCR;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (twoPlayerMenuIndex == 1) {
                        twoPlayerVictoriesIndex++;
                        if (twoPlayerVictoriesIndex > 17) twoPlayerVictoriesIndex = 0;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (twoPlayerMenuIndex == 2 || twoPlayerMenuIndex == 3) {
                        int pi = twoPlayerMenuIndex - 2;
                        playerColorCounts[pi]++;
                        if (playerColorCounts[pi] > 8) playerColorCounts[pi] = 5;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (twoPlayerMenuIndex == 4) {
                        // Start game!
                        chainReaction = twoPlayerCR;
                        AudioMixer::Instance()->PlaySFX("menu_selected");
                        delayTime = 60;
                        runDelay = true;
                    }
                    break;
                }
            }

            switch(e->key.key) {
                case SDLK_UP:
                    // LAN menu navigation (0 = Set Name, 1 = Host, 2+ = servers)
                    if (showingNetPanel && !networkInLobby && networkInputMode == 7) {
                        if (lanMenuIndex > 0) { lanMenuIndex--; AudioMixer::Instance()->PlaySFX("menu_change"); }
                        break;
                    }
                    // Net game menu navigation (0 = Manual entry, 1..n = public servers, n+1 = Set Name)
                    if (showingNetPanel && !networkInLobby && networkInputMode == 10) {
                        if (netMenuIndex > 0) { netMenuIndex--; AudioMixer::Instance()->PlaySFX("menu_change"); }
                        break;
                    }
                    // Handle action menu selection in network lobby
                    if (showingNetPanel && networkInLobby && networkInputMode == 0) {
                        NetworkClient* netClient = NetworkClient::Instance();
                        if (netClient->GetState() == CONNECTED || netClient->GetState() == IN_LOBBY) {
                            GameRoom* currentGame = netClient->GetCurrentGame();

                            // Joiners in game room cannot scroll - stay on chat
                            if (currentGame && currentGame->creator != netClient->GetPlayerNick()) {
                                // Non-host in game - don't allow scrolling
                                break;
                            }

                            int maxActions;
                            if (currentGame && currentGame->creator == netClient->GetPlayerNick()) {
                                // Host: Chat + 4 global + 3 grid rows + Mouse/Touch + optional Start
                                maxActions = 9 + ((int)currentGame->players.size() > 1 ? 1 : 0);
                            } else if (currentGame) {
                                // Non-host in game
                                maxActions = 1; // Just Chat (use ESC to leave)
                            } else {
                                // In lobby
                                std::vector<GameRoom> games = netClient->GetGameList();
                                maxActions = 2 + games.size(); // Chat + Create + Join games
                            }
                            selectedActionIndex--;
                            if (selectedActionIndex < 0) selectedActionIndex = maxActions - 1;
                            AudioMixer::Instance()->PlaySFX("menu_change");
                        }
                    } else {
                        up();
                    }
                    break;
                case SDLK_DOWN:
                    // LAN menu navigation
                    if (showingNetPanel && !networkInLobby && networkInputMode == 7) {
                        int lanMenuMax = 2 + (int)discoveredServers.size(); // 0=Host, 1..n=servers, n+1=SetName
                        if (lanMenuIndex < lanMenuMax - 1) { lanMenuIndex++; AudioMixer::Instance()->PlaySFX("menu_change"); }
                        break;
                    }
                    // Net game menu navigation
                    if (showingNetPanel && !networkInLobby && networkInputMode == 10) {
                        int netMenuMax = 2 + (int)publicServers.size(); // 0=Manual, 1..n=servers, n+1=SetName
                        if (netMenuIndex < netMenuMax - 1) { netMenuIndex++; AudioMixer::Instance()->PlaySFX("menu_change"); }
                        break;
                    }
                    // Handle action menu selection in network lobby
                    if (showingNetPanel && networkInLobby && networkInputMode == 0) {
                        NetworkClient* netClient = NetworkClient::Instance();
                        if (netClient->GetState() == CONNECTED || netClient->GetState() == IN_LOBBY) {
                            GameRoom* currentGame = netClient->GetCurrentGame();

                            // Joiners in game room cannot scroll - stay on chat
                            if (currentGame && currentGame->creator != netClient->GetPlayerNick()) {
                                // Non-host in game - don't allow scrolling
                                break;
                            }

                            int maxActions;
                            if (currentGame && currentGame->creator == netClient->GetPlayerNick()) {
                                // Host: Chat + 4 global + 3 grid rows + Mouse/Touch + optional Start
                                maxActions = 9 + ((int)currentGame->players.size() > 1 ? 1 : 0);
                            } else if (currentGame) {
                                // Non-host in game
                                maxActions = 1; // Just Chat (use ESC to leave)
                            } else {
                                // In lobby
                                std::vector<GameRoom> games = netClient->GetGameList();
                                maxActions = 2 + games.size(); // Chat + Create + Join games
                            }
                            selectedActionIndex++;
                            if (selectedActionIndex >= maxActions) selectedActionIndex = 0;
                            AudioMixer::Instance()->PlaySFX("menu_change");
                        }
                    } else {
                        down();
                    }
                    break;
                case SDLK_LEFT:
                case SDLK_RIGHT:
                    // Handle LEFT/RIGHT for changing settings values (when not Chat)
                    if (showingNetPanel && networkInLobby && networkInputMode == 0 && selectedActionIndex != 0) {
                        NetworkClient* netClient = NetworkClient::Instance();
                        GameRoom* currentGame = netClient->GetCurrentGame();
                        if (currentGame && currentGame->creator == netClient->GetPlayerNick()) {
                            // Only host can change settings
                            bool settingChanged = false;
                            if (selectedActionIndex == 1) {
                                chainReactionEnabled = !chainReactionEnabled;
                                AudioMixer::Instance()->PlaySFX("menu_change");
                                settingChanged = true;
                            } else if (selectedActionIndex == 2) {
                                continueWhenPlayersLeave = !continueWhenPlayersLeave;
                                AudioMixer::Instance()->PlaySFX("menu_change");
                                settingChanged = true;
                            } else if (selectedActionIndex == 3) {
                                singlePlayerTargetting = !singlePlayerTargetting;
                                AudioMixer::Instance()->PlaySFX("menu_change");
                                settingChanged = true;
                            } else if (selectedActionIndex == 4) {
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
                            } else if (selectedActionIndex >= 6 && selectedActionIndex <= 8) {
                                // Grid rows 6/7/8: Left/Right navigates player columns
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
                            }
                            if (settingChanged) {
                                static const int vLimits[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,15,20,30,50,100};
                                netClient->SendOptions(chainReactionEnabled, continueWhenPlayersLeave,
                                    singlePlayerTargetting, vLimits[victoriesLimitIndex], playerColorCounts, playerNoCompress, playerAimGuide, netRoomMouseEnabled);
                            }
                        }
                    }
                    break;
                case SDLK_RETURN:
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
                        break;
                    }
                    if (showingNetPanel && networkInLobby && networkInputMode == 0) {
                        // Execute selected action in main lobby or game room
                        NetworkClient* netClient = NetworkClient::Instance();
                        if (netClient->GetState() == CONNECTED || netClient->GetState() == IN_LOBBY) {
                            GameRoom* currentGame = netClient->GetCurrentGame();

                            if (selectedActionIndex == 0) {
#ifdef __WASM_PORT__
                                // Touch devices can't type into the inline chat field
                                // (SDL3's Emscripten backend can't summon the soft
                                // keyboard) — compose in a native browser prompt.
                                if (WasmHasTouch() && networkChatInput[0] == '\0') {
                                    WasmPromptText("Chat message:", "", networkChatInput, sizeof(networkChatInput));
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
                                        } else if (strcmp(networkChatInput, "/help") == 0) {
                                            // Show help
                                            netClient->AddStatusMessage("Available commands: /nick <new_nick>, /help");
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
                            } else if (currentGame) {
                                // In game room - handle game settings and Start/Part actions
                                bool isHost = currentGame->creator == netClient->GetPlayerNick();

                                if (selectedActionIndex == 0) {
#ifdef __WASM_PORT__
                                    // Same soft-keyboard workaround as the lobby chat above.
                                    if (WasmHasTouch() && networkChatInput[0] == '\0') {
                                        WasmPromptText("Chat message:", "", networkChatInput, sizeof(networkChatInput));
                                    }
#endif
                                    // Chat - same for both host and joiner
                                    if (strlen(networkChatInput) > 0) {
                                        if (networkChatInput[0] == '/') {
                                            // Handle chat commands
                                            if (strncmp(networkChatInput, "/nick ", 6) == 0) {
                                                const char* newNick = networkChatInput + 6;
                                                if (strlen(newNick) > 0 && netClient->SendNick(newNick)) {
                                                    char msg[128];
                                                    snprintf(msg, sizeof(msg), "You are now known as %s", newNick);
                                                    netClient->AddStatusMessage(msg);
                                                } else {
                                                    netClient->AddStatusMessage("Erroneous nickname");
                                                }
                                            } else if (strcmp(networkChatInput, "/help") == 0) {
                                                netClient->AddStatusMessage("Available commands: /nick <new_nick>, /help");
                                            } else {
                                                netClient->AddStatusMessage("Unknown command. Type /help for help.");
                                            }
                                        } else {
                                            // Regular chat message
                                            netClient->SendTalk(networkChatInput);
                                        }
                                        networkChatInput[0] = '\0';
                                        AudioMixer::Instance()->PlaySFX("menu_selected");
                                    }
                                } else if (isHost) {
                                    // Host actions: 0=Chat, 1=CR, 2=Continue, 3=Target, 4=Victories, 5..5+N-1=Colors, last=Start
                                    int numPlayers = currentGame ? (int)currentGame->players.size() : 1;
                                    if (numPlayers < 1) numPlayers = 1;
                                    if (numPlayers > 5) numPlayers = 5;
                                    bool settingChanged = false;
                                    if (selectedActionIndex == 1) {
                                        // Toggle chain reaction
                                        chainReactionEnabled = !chainReactionEnabled;
                                        AudioMixer::Instance()->PlaySFX("menu_change");
                                        settingChanged = true;
                                    } else if (selectedActionIndex == 2) {
                                        // Toggle continue when players leave
                                        continueWhenPlayersLeave = !continueWhenPlayersLeave;
                                        AudioMixer::Instance()->PlaySFX("menu_change");
                                        settingChanged = true;
                                    } else if (selectedActionIndex == 3) {
                                        // Toggle single player targetting
                                        singlePlayerTargetting = !singlePlayerTargetting;
                                        AudioMixer::Instance()->PlaySFX("menu_change");
                                        settingChanged = true;
                                    } else if (selectedActionIndex == 4) {
                                        // Cycle victories limit
                                        victoriesLimitIndex++;
                                        if (victoriesLimitIndex > 17) victoriesLimitIndex = 0; // 18 values total
                                        AudioMixer::Instance()->PlaySFX("menu_change");
                                        settingChanged = true;
                                    } else if (selectedActionIndex == 5) {
                                        // Toggle mouse/touch aim (per-session, off by default)
                                        netRoomMouseEnabled = !netRoomMouseEnabled;
                                        AudioMixer::Instance()->PlaySFX("menu_change");
                                    } else if (selectedActionIndex == 6) {
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
                                    } else if (selectedActionIndex == 7) {
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
                                    } else if (selectedActionIndex == 8) {
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
                                    } else if (selectedActionIndex == 9 && currentGame && currentGame->players.size() > 1) {
                                        // Start game (index 9, fixed regardless of player count)
                                        netClient->StartGame();
                                        netClient->AddStatusMessage("Starting game...");
                                        AudioMixer::Instance()->PlaySFX("menu_selected");
                                    }
                                    if (settingChanged) {
                                        static const int vLimits[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,15,20,30,50,100};
                                        netClient->SendOptions(chainReactionEnabled, continueWhenPlayersLeave,
                                            singlePlayerTargetting, vLimits[victoriesLimitIndex], playerColorCounts, playerNoCompress, playerAimGuide, netRoomMouseEnabled);
                                    }
                                }
                                // Non-host has no actions other than Chat (use ESC to leave)
                            } else {
                                // In lobby - handle Create/Join actions
                                if (selectedActionIndex == 1) {
                                    // Create new game - do it immediately like original
                                    netClient->CreateGame();
                                    netClient->AddStatusMessage("Game created - now you need to wait for players to join");
                                    AudioMixer::Instance()->PlaySFX("menu_selected");
                                } else {
                                    // Join game (selectedActionIndex >= 2)
                                    SDL_Log("Join game action: selectedActionIndex=%d", selectedActionIndex);
                                    std::vector<GameRoom> games = netClient->GetGameList();
                                    int gameIndex = selectedActionIndex - 2;
                                    SDL_Log("Join game: gameIndex=%d, games.size()=%d", gameIndex, (int)games.size());
                                    if (gameIndex >= 0 && gameIndex < (int)games.size()) {
                                        SDL_Log("Attempting to join game created by: %s", games[gameIndex].creator.c_str());
                                        if (netClient->JoinGame(games[gameIndex].creator.c_str())) {
                                            SDL_Log("JoinGame returned true - successfully joined!");
                                            netClient->AddStatusMessage("Joined game");
                                            AudioMixer::Instance()->PlaySFX("menu_selected");
                                            // Joiner defaults to chat (index 0) and can't scroll through settings
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
                        break;
                    } else if (showingNetPanel && networkInLobby && networkInputMode == 6) {
                        // Create game confirmed
                        NetworkClient* netClient = NetworkClient::Instance();
                        // Accept CONNECTED or IN_LOBBY — after returning from a game state is IN_LOBBY
                        if (netClient->GetState() == CONNECTED || netClient->GetState() == IN_LOBBY) {
                            if (netClient->CreateGame()) {
                                AudioMixer::Instance()->PlaySFX("menu_selected");
                            }
                        }
                        networkInputMode = 0; // Back to lobby
                        SDL_StopTextInput(SDL_GetKeyboardFocus());
                        break;
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
                        break;
                    } else if (showingNetPanel && networkInLobby && networkInputMode == 4) {
                        // Send chat message
                        if (strlen(networkChatInput) > 0) {
                            NetworkClient* netClient = NetworkClient::Instance();
                            netClient->SendTalk(networkChatInput);
                            networkChatInput[0] = '\0';
                        }
                        networkInputMode = 0; // Back to lobby
                        SDL_StopTextInput(SDL_GetKeyboardFocus());
                        break;
                    } else if (showingNetPanel && networkInLobby && networkInputMode == 3) {
                        // Join game with entered creator name
                        if (strlen(networkJoinCreator) > 0) {
                            NetworkClient* netClient = NetworkClient::Instance();
                            if (netClient->JoinGame(networkJoinCreator)) {
                                SDL_Log("Joined game created by %s", networkJoinCreator);
                            }
                            networkInputMode = 0; // Back to lobby
                        }
                        break;
                    } else if (showingNetPanel && !networkInLobby && networkInputMode == 11) {
                        // Confirm pre-lobby nickname
                        networkInputMode = networkPreNickReturnMode;
                        SDL_StopTextInput(SDL_GetKeyboardFocus());
                        AudioMixer::Instance()->PlaySFX("menu_selected");
                        SavePreNick();
                        break;
                    } else if (showingNetPanel && !networkInLobby &&
                               (networkInputMode == 8 || networkInputMode == 9)) {
                        if (networkFieldEditing) {
                            // ENTER while keyboard open → close keyboard
                            networkFieldEditing = false;
                            SDL_StopTextInput(SDL_GetKeyboardFocus());
                            AudioMixer::Instance()->PlaySFX("menu_selected");
                            break;
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
                                break;
                            }
#endif
                            // ENTER on host/port field → open keyboard
                            networkFieldEditing = true;
                            SDL_StopTextInput(SDL_GetKeyboardFocus());
                            SDL_StartTextInput(SDL_GetKeyboardFocus());
                            { SDL_Rect r = {160, 152, 320, 20}; SDL_SetTextInputArea(SDL_GetKeyboardFocus(), &r, 0); }
                            AudioMixer::Instance()->PlaySFX("menu_selected");
                            break;
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
                                    SDL_Delay(500);
                                }
                                connectErrorMsg.clear();
                                discoveredServers = NetworkClient::DiscoverLANServers();
                                for (auto& s : discoveredServers)
                                    s.latencyMs = NetworkClient::MeasureLatency(s.host.c_str(), s.port);
                                lanMenuIndex = 0;
                                break;
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
                                    break;
                                }
#endif
                                networkPreNick[0] = '\0';
                                networkPreNickReturnMode = 7;
                                networkInputMode = 11;
                                SDL_StopTextInput(SDL_GetKeyboardFocus());
                                SDL_StartTextInput(SDL_GetKeyboardFocus());
                                { SDL_Rect r = {160, 152, 320, 20}; SDL_SetTextInputArea(SDL_GetKeyboardFocus(), &r, 0); }
                                AudioMixer::Instance()->PlaySFX("menu_selected");
                                break;
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
                                break;
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
                                    break;
                                }
#endif
                                networkPreNick[0] = '\0';
                                networkPreNickReturnMode = 10;
                                networkInputMode = 11;
                                SDL_StopTextInput(SDL_GetKeyboardFocus());
                                SDL_StartTextInput(SDL_GetKeyboardFocus());
                                { SDL_Rect r = {160, 152, 320, 20}; SDL_SetTextInputArea(SDL_GetKeyboardFocus(), &r, 0); }
                                AudioMixer::Instance()->PlaySFX("menu_selected");
                                break;
                            }
                            host = publicServers[serverIdx].host.c_str();
                            port = publicServers[serverIdx].port;
                        }
                        NetworkClient* netClient = NetworkClient::Instance(host, port);
                        if (netClient->IsConnected()) {
                            AudioMixer::Instance()->PlaySFX("menu_selected");
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
                                SDL_Delay(100);
                                std::string geoLoc = NetworkClient::DetectGeoLocation();
                                // Parse for own spot rendering
                                float gLat = 0.0f, gLon = 0.0f;
                                if (sscanf(geoLoc.c_str(), "%f:%f", &gLat, &gLon) == 2) {
                                    myGeoLat = gLat; myGeoLon = gLon; myGeoLocSet = true;
                                }
                                if (netClient->SendGeoLoc(geoLoc.c_str())) {
                                    networkInLobby = true;
                                    networkInputMode = 0;  // Switch to lobby mode so C/J/T/U keys work
                                    networkGameStarting = false;
                                    wasmSyncWaitStart = 0;
                                    netClient->RequestList();  // Immediate list on lobby entry
                                    lastListRequest = SDL_GetTicks();
#ifdef __ANDROID__
                                    SDL_SendAndroidMessage(0x8001, 0); // show lobby ad
#endif
                                }
                            } else {
                                // SendNick failed — WebSocket is still connecting (WASM async).
                                // Store the nickname and complete lobby entry in NetPanelRender()
                                // once the WebSocket open callback fires and state becomes CONNECTED.
                                SDL_Log("SendNick failed (state=%d), setting pendingLobbyConnect for async completion", netClient->GetState());
                                snprintf(networkPreNick, sizeof(networkPreNick), "%s", nickname);
                                pendingLobbyConnect = true;
                                connectErrorMsg.clear();
                            }
                        } else {
                            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to connect to %s:%d", host, port);
                            char errBuf[320];
                            snprintf(errBuf, sizeof(errBuf), "Failed to connect to %s:%d", host, port);
                            connectErrorMsg = errBuf;
                        }
                    } else {
                        press();
                    }
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
                    if (showingNetPanel && !networkInLobby && networkInputMode == 7) {
                        connectErrorMsg.clear();
                        discoveredServers = NetworkClient::DiscoverLANServers();
                        for (auto& s : discoveredServers)
                            s.latencyMs = NetworkClient::MeasureLatency(s.host.c_str(), s.port);
                        lanMenuIndex = 0;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    }
                    if (showingNetPanel && !networkInLobby && networkInputMode == 10) {
                        connectErrorMsg.clear();
                        netMenuIndex = 0;
                        publicServers = NetworkClient::FetchPublicServers();
                        for (auto& s : publicServers)
                            s.latencyMs = NetworkClient::MeasureLatency(s.host.c_str(), s.port);
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
                        { SDL_Rect r = {160, 152, 320, 20}; SDL_SetTextInputArea(SDL_GetKeyboardFocus(), &r, 0); }
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
                        { SDL_Rect r = {160, 152, 320, 20}; SDL_SetTextInputArea(SDL_GetKeyboardFocus(), &r, 0); }
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
                    if (showingSPPanel) {
                        AudioMixer::Instance()->PlaySFX("cancel");
                        showingSPPanel = false;
                        break;
                    }
                    if (showingNetPanel) {
                        AudioMixer::Instance()->PlaySFX("cancel");
                        if (networkInputMode == 11) {
                            // Cancel pre-lobby nickname editing
                            networkInputMode = networkPreNickReturnMode;
                            SDL_StopTextInput(SDL_GetKeyboardFocus());
                            break;
                        } else if (networkInputMode == 8 || networkInputMode == 9) {
                            if (networkFieldEditing) {
                                // ESC while keyboard is open: close keyboard, stay on form
                                networkFieldEditing = false;
                                SDL_StopTextInput(SDL_GetKeyboardFocus());
                            } else {
                                // ESC with no keyboard: back to public server list
                                networkInputMode = 10;
                            }
                            break;
                        } else if (networkInputMode == 10) {
                            // Close net panel
                            showingNetPanel = false;
                            break;
                        } else if (networkInputMode == 6) {
                            // Cancel create game confirmation
                            networkInputMode = 0;
                            break;
                        } else if (networkInputMode == 5) {
                            // Cancel username input
                            networkInputMode = 0;
                            networkUsername[0] = '\0';
                            SDL_StopTextInput(SDL_GetKeyboardFocus());
                            break;
                        } else if (networkInputMode == 4) {
                            // Cancel chat input
                            networkInputMode = 0;
                            networkChatInput[0] = '\0';
                            break;
                        } else if (networkInputMode == 3) {
                            // Cancel join input
                            networkInputMode = 0;
                            networkJoinCreator[0] = '\0';
                            break;
                        } else if (networkInputMode == 0 && networkInLobby) {
                            // In lobby - check if in a game
                            NetworkClient* netClient = NetworkClient::Instance();
                            GameRoom* currentGame = netClient->GetCurrentGame();
                            if (currentGame) {
                                // Leave the game (like original)
                                netClient->PartGame();
                                netClient->RequestList();  // Immediate list after parting
                                lastListRequest = SDL_GetTicks();
                                netClient->AddStatusMessage("*** Leaving game...");
                            } else {
                                // Not in a game - disconnect from server
                                showingNetPanel = false;
                                networkInLobby = false;
                                if (netClient->IsConnected()) {
                                    netClient->Disconnect();
                                }
                            }
                        } else {
                            showingNetPanel = false;
                            networkInLobby = false;
                            pendingLobbyConnect = false;
                            if (NetworkClient::Instance()->IsConnected()) {
                                NetworkClient::Instance()->Disconnect();
                            }
                            if (serverHosting) {
                                StopLocalServer();
                            }
                        }
                        break;
                    }
                    if (showingLevelPanel) {
                        AudioMixer::Instance()->PlaySFX("cancel");
                        showingLevelPanel = false;
                        SDL_StopTextInput(SDL_GetKeyboardFocus());
                        runDelay = false;
                        break;
                    }
                    if (showingKeysPanel) {
                        AudioMixer::Instance()->PlaySFX("cancel");
                        showingKeysPanel = false;
                        GameSettings::Instance()->SaveKeys();
                        break;
                    }
                    if (showing2PPanel) {
                        AudioMixer::Instance()->PlaySFX("cancel");
                        showing2PPanel = false;
                        break;
                    }
                    if (showingLocalMPPanel) {
                        AudioMixer::Instance()->PlaySFX("cancel");
                        showingLocalMPPanel = false;
                        break;
                    }
                    if (showingOptPanel || showingNetSetupPanel) {
                        AudioMixer::Instance()->PlaySFX("cancel");
                        showingOptPanel = showingNetSetupPanel = false;
                        awaitKp = false;
                        break;
                    }
                    // On Android TV, pressing Back at the root menu should not
                    // quit the game — the Fire TV launcher injects KEYCODE_BACK
                    // during the launch transition, which would immediately kill
                    // the app. Users navigate away via the Home button instead.
#ifndef __ANDROID__
                    FrozenBubble::Instance()->CallGameQuit();
#endif
                    break;
                case SDLK_F11: // mute / unpause audio
                    if(AudioMixer::Instance()->IsHalted() == true) {
                        AudioMixer::Instance()->MuteAll(true);
                        AudioMixer::Instance()->PlayMusic("intro");
                    }
                    else AudioMixer::Instance()->MuteAll();
                    break;
            }
            break;
    }
}

